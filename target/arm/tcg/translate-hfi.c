/*
 * AArch64 HFI (Hardware Fault Isolation) translation
 *
 * Copyright (c) 2024
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

// clang-format off
#include "translate-hfi.h"
#include "qemu/osdep.h"
#include "../hfi.h"

#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "translate.h"
#include "translate-a64.h"
#include "qemu/log.h"
#include "arm_ldst.h"
#include "semihosting/semihost.h"
#include "cpregs.h"
// clang-format on

// =============================================================================
// ================================= MACROS ====================================
// =============================================================================

#define HFI_GLOBAL_TEMP_INIT static CPUArchState_HFI* hfi = (void*)(0)
#define HFI_GLOBAL_TEMP_STRUCT_NAME tcg_hfi
#define HFI_OFFSET_OF(field) (((intptr_t)((void*)(&field) - (void*)hfi)) + offsetof(CPUARMState, hfi))

#define HFI_NEW_GLOBAL(func, field) (func(tcg_env,              \
                                          HFI_OFFSET_OF(field), \
                                          "hfi." #field))

#define HFI_NEW_GLOBAL_I64(field) HFI_NEW_GLOBAL(tcg_global_mem_new_i64, field)
#define HFI_NEW_GLOBAL_I32(field) HFI_NEW_GLOBAL(tcg_global_mem_new_i32, field)
#define HFI_NEW_GLOBAL_PTR(field) HFI_NEW_GLOBAL(tcg_global_mem_new_ptr, field)

/* Auto-detect field type and initialize TCG global with appropriate function */
#define HFI_INIT_FIELD(field_path)                                                              \
    HFI_GLOBAL_TEMP_STRUCT_NAME.field_path = _Generic((HFI_GLOBAL_TEMP_STRUCT_NAME.field_path), \
        TCGv_i64: HFI_NEW_GLOBAL_I64(hfi->field_path),                                          \
        TCGv_i32: HFI_NEW_GLOBAL_I32(hfi->field_path),                                          \
        TCGv_ptr: HFI_NEW_GLOBAL_PTR(hfi->field_path))

// =============================================================================
// ============================= INITIALIZATION ================================
// =============================================================================

static struct {
    struct {
        TCGv_i64 reg_base;
        TCGv_i64 reg_mask_or_bound;
        TCGv_i32 reg_perms_flags;
    } regions[HFI_TOTAL_REGIONS];
    struct {
        TCGv_i64 reg_fault_state;
        TCGv_i64 reg_exit_state;
    } exec_state;
    struct {
        TCGv_i32 reg_enabled;
        TCGv_i32 reg_config_opts;
        TCGv_ptr reg_exit_handler_addr;
    } control_config;
} HFI_GLOBAL_TEMP_STRUCT_NAME;

HFI_GLOBAL_TEMP_INIT;

void hfi_translate_init(void) {
    int i;

    /* Initialize all regions */
    for (i = 0; i < HFI_TOTAL_REGIONS; i++) {
        HFI_INIT_FIELD(regions[i].reg_base);
        HFI_INIT_FIELD(regions[i].reg_mask_or_bound);
        HFI_INIT_FIELD(regions[i].reg_perms_flags);
    }

    /* Initialize exec state (fault and exit state bitvectors) */
    HFI_INIT_FIELD(exec_state.reg_fault_state);
    HFI_INIT_FIELD(exec_state.reg_exit_state);

    /* Initialize control configuration */
    HFI_INIT_FIELD(control_config.reg_enabled);
    HFI_INIT_FIELD(control_config.reg_config_opts);
    HFI_INIT_FIELD(control_config.reg_exit_handler_addr);
}

TCGv_i64 read_explicit_region_reg(DisasContext *s, u_int8_t reg_num)
{
    TCGv_i64 v = tcg_temp_new_i64();
    tcg_gen_mov_i64(tcg_hfi.regions[reg_num + 4].reg_base, v);
    return v;
}

// =============================================================================
// =============================== HELPERS =====================================
// =============================================================================

// /**
//  * Abstracted check patterns at the translation IR level
//  * See helper-hfi.c for gen level helpers
//  * Usage pattern:
//  *   TCGLabel* skip_label;
//  *   hfi_begin_region_locked(&skip_label);
//  *   ... code that should only execute if HFI is enabled and region-locked ...
//  *   hfi_end_section(skip_label);
//  */

// /* Begin a region-locked section (checks enabled AND locked) */
// static void hfi_begin_region_locked(TCGLabel** skip_label) {
//     TCGv_i32 enabled = tcg_temp_new_i32();
//     TCGv_i32 locked = tcg_temp_new_i32();
//     *skip_label = gen_new_label();

//     tcg_gen_mov_i32(enabled, tcg_hfi.control_config.reg_enabled);
//     tcg_gen_mov_i32(locked, tcg_hfi.control_config.reg_config_opts);
//     tcg_gen_andi_i32(locked, locked, 0x1); /* Extract bit 0 for lock state */

//     TCGv_i32 cond = tcg_temp_new_i32();
//     tcg_gen_and_i32(cond, enabled, locked);
//     tcg_gen_brcondi_i32(TCG_COND_EQ, cond, 0, *skip_label);
// }

// /* Begin an enabled section (checks enabled only) */
// static void hfi_begin_enabled(TCGLabel** skip_label) {
//     TCGv_i32 enabled = tcg_temp_new_i32();
//     *skip_label = gen_new_label();

//     tcg_gen_mov_i32(enabled, tcg_hfi.control_config.reg_enabled);
//     tcg_gen_brcondi_i32(TCG_COND_EQ, enabled, 0, *skip_label);
// }

// /* End a conditional section */
// static void hfi_end_section(TCGLabel* skip_label) {
//     gen_set_label(skip_label);
// }

// =============================================================================
// =============================== TRANSLATION =================================
// =============================================================================

/*
 * Include the generated decoder.
 */

#include "decode-hfi.c.inc"

static bool trans_HFI_SRB(DisasContext* ctx, arg_HFI_SRB* a) {
    gen_helper_hfi_srb(tcg_env, cpu_reg(ctx, a->rn), cpu_reg(ctx, a->gpr));
    return true;
}

static bool trans_HFI_GRB(DisasContext* ctx, arg_HFI_GRB* a) {
    gen_helper_hfi_grb(cpu_reg(ctx, a->gpr), tcg_env, cpu_reg(ctx, a->rn));
    return true;
}

static bool trans_HFI_SRM(DisasContext* ctx, arg_HFI_SRM* a) {
    gen_helper_hfi_srm(tcg_env, cpu_reg(ctx, a->rn), cpu_reg(ctx, a->gpr));
    return true;
}

static bool trans_HFI_GRM(DisasContext* ctx, arg_HFI_GRM* a) {
    gen_helper_hfi_grm(cpu_reg(ctx, a->gpr), tcg_env, cpu_reg(ctx, a->rn));
    return true;
}

static bool trans_HFI_SRP(DisasContext* ctx, arg_HFI_SRP* a) {
    gen_helper_hfi_srp(tcg_env, cpu_reg(ctx, a->rn), cpu_reg(ctx, a->gpr));
    return true;
}

static bool trans_HFI_GRP(DisasContext* ctx, arg_HFI_GRP* a) {
    gen_helper_hfi_grp(cpu_reg(ctx, a->gpr), tcg_env, cpu_reg(ctx, a->rn));
    return true;
}

static bool trans_HFI_SEH(DisasContext* ctx, arg_HFI_SEH* a) {
    gen_helper_hfi_seh(tcg_env, cpu_reg(ctx, a->gpr));
    return true;
}

static bool trans_HFI_GEH(DisasContext* ctx, arg_HFI_GEH* a) {
    gen_helper_hfi_geh(cpu_reg(ctx, a->gpr), tcg_env);
    return true;
}

static bool trans_HFI_GFS(DisasContext* ctx, arg_HFI_GFS* a) {
    gen_helper_hfi_gfs(cpu_reg(ctx, a->gpr), tcg_env);
    return true;
}

static bool trans_HFI_SFS(DisasContext* ctx, arg_HFI_SFS* a) {
    gen_helper_hfi_sfs(tcg_env, cpu_reg(ctx, a->gpr));
    return true;
}

static bool trans_HFI_GES(DisasContext* ctx, arg_HFI_GES* a) {
    gen_helper_hfi_ges(cpu_reg(ctx, a->gpr), tcg_env);
    return true;
}

static bool trans_HFI_SES(DisasContext* ctx, arg_HFI_SES* a) {
    gen_helper_hfi_ses(tcg_env, cpu_reg(ctx, a->gpr));
    return true;
}

static bool trans_HFI_ENTER(DisasContext* ctx, arg_HFI_ENTER* a) {
    gen_helper_hfi_enter(tcg_env, cpu_reg(ctx, a->gpr), cpu_reg(ctx, a->optr));
    return true;
}

static bool trans_HFI_EXIT(DisasContext* ctx, arg_HFI_EXIT* a) {
    gen_helper_hfi_exit(tcg_env, tcg_constant_i32(HFI_EXIT_CALLED));
    return true;
}

// =============================================================================

// OLD to be rmeoved

/*
 * Implement all of the translator functions referenced by the decoder.
 */

static bool trans_CSTM(DisasContext* ctx, arg_CSTM* a) {
    /*
     * CSTM: Check System Tag in Memory
     * Reads from region specified by rn and masks with (2^12 - 1)
     */
    TCGv_i64 tcg_rn = cpu_reg_sp(ctx, a->rn); /* Read source */
    TCGv_i64 tcg_rd = cpu_reg(ctx, a->rd);    /* Get destination */

    /* Mask operation: keep lower 12 bits */
    tcg_gen_andi_i64(tcg_rd, tcg_rn, (1 << 12) - 1);

    return true;
}

static bool trans_HFI_SR(DisasContext* ctx, arg_HFI_SR* a) {
    /*
     * HFI_SR: HFI Set Region
     * Load two 64-bit HFI region descriptors from memory
     * The address is in register specified by region_ptr_gpr
     * Reads two 64-bit values at [rn] and [rn+8]
     * Writes to the HFI region registers hfi.regions[region_number]
     */

    TCGv_i64 region_ptr = cpu_reg_sp(ctx, a->region_ptr_gpr);
    TCGv_i64 value1 = tcg_temp_new_i64();
    TCGv_i64 value2 = tcg_temp_new_i64();

    /*
     * Get memory access index for the current translation context.
     * This determines the appropriate memory access callbacks and
     * privilege level (user vs. supervisor mode) for load/store operations.
     * Essential for proper MMU handling and access control during instruction translation.
     */
    int memidx = get_mem_index(ctx);
    MemOp memop = MO_64 | MO_ALIGN;

    /* Read first 64-bit value from [region_ptr] (region_base_ptr) */
    tcg_gen_qemu_ld_i64(value1, region_ptr, memidx, memop);

    /* Read second 64-bit value from [region_ptr+8] */
    TCGv_i64 addr2 = tcg_temp_new_i64();
    tcg_gen_addi_i64(addr2, region_ptr, 8);
    tcg_gen_qemu_ld_i64(value2, addr2, memidx, memop);

    /* Get region number as a constant for array indexing */
    uint32_t region_number = a->region_number;

    /*
     * Write to HFI region base and mask/bound registers in CPUARMState
     * Using the unified regions array structure
     */
    if (region_number < HFI_TOTAL_REGIONS) {
        tcg_gen_mov_i64(tcg_hfi.regions[region_number].reg_base, value1);
        tcg_gen_mov_i64(tcg_hfi.regions[region_number].reg_mask_or_bound, value2);
    }

    /* Temporary registers are freed automatically by TCG */
    return true;
}

static void op_addr_hldst_imm_pre(DisasContext *s, arg_hldst_imm *a,
                                 TCGv_i64 *clean_addr, TCGv_i64 *dirty_addr,
                                 uint64_t offset, bool is_store, MemOp mop)
{
    int memidx;
    u_int8_t region_num = a->rn1 << 1 | a->rn2;


    *dirty_addr = read_explicit_region_reg(s, region_num)
    if (!a->p) {
        tcg_gen_addi_i64(*dirty_addr, *dirty_addr, offset);
    }
    memidx = get_a64_user_mem_index(s, a->unpriv);
    *clean_addr = gen_mte_check1_mmuidx(s, *dirty_addr, is_store,
                                        a->w,
                                        mop, a->unpriv, memidx);
}

static void op_addr_hldst_imm_post(DisasContext *s, arg_hldst_imm *a,
                                  TCGv_i64 dirty_addr, uint64_t offset)
{
    if (a->w) {
        if (a->p) {
            tcg_gen_addi_i64(dirty_addr, dirty_addr, offset);
        }
    }
}

static bool trans_HSTR_i(DisasContext *s, arg_hldst_imm *a)
{
    bool iss_sf, iss_valid = !a->w;
    TCGv_i64 clean_addr, dirty_addr, tcg_rt;
    int memidx = get_a64_user_mem_index(s, a->unpriv);
    MemOp mop = finalize_memop(s, a->sz);

    op_addr_hldst_imm_pre(s, a, &clean_addr, &dirty_addr, a->imm, true, mop);

    tcg_rt = cpu_reg(s, a->rt);
    iss_sf = ldst_iss_sf(a->sz, 0, 0);

    do_gpr_st_memidx(s, tcg_rt, clean_addr, mop, memidx,
                     iss_valid, a->rt, iss_sf, false);
    op_addr_hldst_imm_post(s, a, dirty_addr, a->imm);
    return true;
}

static bool trans_HLDR_i(DisasContext *s, arg_hldst_imm *a)
{
    bool iss_sf, iss_valid = !a->w;
    TCGv_i64 clean_addr, dirty_addr, tcg_rt;
    int memidx = get_a64_user_mem_index(s, a->unpriv);
    MemOp mop = finalize_memop(s, a->sz);

    op_addr_hldst_imm_pre(s, a, &clean_addr, &dirty_addr, a->imm, false, mop);

    tcg_rt = cpu_reg(s, a->rt);
    iss_sf = ldst_iss_sf(a->sz, 0, 0);

    do_gpr_ld_memidx(s, tcg_rt, clean_addr, mop,
                     a->ext, memidx, iss_valid, a->rt, iss_sf, false);
    op_addr_hldst_imm_post(s, a, dirty_addr, a->imm);
    return true;
}

