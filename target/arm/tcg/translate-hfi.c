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

#include "exec/exec-all.h"
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
        struct {
            TCGv_i64 reg_base_addr;
            TCGv_i64 reg_lsb_mask;
            TCGv_i32 reg_perm_exec;
        } implicit_code[2];
        struct {
            TCGv_i64 reg_base_addr;
            TCGv_i64 reg_lsb_mask;
            TCGv_i32 reg_perm_read;
            TCGv_i32 reg_perm_write;
        } implicit_data[4];
        struct {
            TCGv_i64 reg_base_addr;
            TCGv_i64 reg_bound_addr;
            TCGv_i32 reg_perm_read;
            TCGv_i32 reg_perm_write;
            TCGv_i32 reg_is_large;
        } explicit_data[4];
    } regions;
    struct {
        TCGv_i32 reg_fault_region_id;
        TCGv_i32 reg_fault_reason;
        TCGv_i32 reg_fault_operation;
        TCGv_i32 reg_fault_occurred;
    } fault_config;
    struct {
        TCGv_ptr reg_exit_handler_addr;
        TCGv_i32 exit_reason;
    } exit_state;
    struct {
        TCGv_i32 reg_enabled;
        TCGv_i32 reg_config_opts;
    } control_config;
} HFI_GLOBAL_TEMP_STRUCT_NAME;

HFI_GLOBAL_TEMP_INIT;

void hfi_translate_init(void) {
    int i;

    /* Initialize implicit code regions */
    for (i = 0; i < 2; i++) {
        HFI_INIT_FIELD(regions.implicit_code[i].reg_base_addr);
        HFI_INIT_FIELD(regions.implicit_code[i].reg_lsb_mask);
        HFI_INIT_FIELD(regions.implicit_code[i].reg_perm_exec);
    }

    /* Initialize implicit data regions */
    for (i = 0; i < 4; i++) {
        HFI_INIT_FIELD(regions.implicit_data[i].reg_base_addr);
        HFI_INIT_FIELD(regions.implicit_data[i].reg_lsb_mask);
        HFI_INIT_FIELD(regions.implicit_data[i].reg_perm_read);
        HFI_INIT_FIELD(regions.implicit_data[i].reg_perm_write);
    }

    /* Initialize explicit data regions */
    for (i = 0; i < 4; i++) {
        HFI_INIT_FIELD(regions.explicit_data[i].reg_base_addr);
        HFI_INIT_FIELD(regions.explicit_data[i].reg_bound_addr);
        HFI_INIT_FIELD(regions.explicit_data[i].reg_perm_read);
        HFI_INIT_FIELD(regions.explicit_data[i].reg_perm_write);
        HFI_INIT_FIELD(regions.explicit_data[i].reg_is_large);
    }

    /* Initialize fault configuration */
    HFI_INIT_FIELD(fault_config.reg_fault_region_id);
    HFI_INIT_FIELD(fault_config.reg_fault_reason);
    HFI_INIT_FIELD(fault_config.reg_fault_operation);
    HFI_INIT_FIELD(fault_config.reg_fault_occurred);

    /* Initialize exit state */
    HFI_INIT_FIELD(exit_state.reg_exit_handler_addr);
    HFI_INIT_FIELD(exit_state.exit_reason);

    /* Initialize control configuration */
    HFI_INIT_FIELD(control_config.reg_enabled);
    HFI_INIT_FIELD(control_config.reg_config_opts);
}

// =============================================================================
// =============================== TRANSLATION =================================
// =============================================================================

/*
 * Include the generated decoder.
 */

#include "decode-hfi.c.inc"

static void nop(DisasContext* ctx) {
    tcg_gen_and_i64(cpu_reg(ctx, 1), cpu_reg(ctx, 1), cpu_reg(ctx, 1));
}


static bool trans_HFI_SRB(DisasContext* ctx, arg_HFI_SRB* a) {
    /* HFI Set Region Base - write gpr_src to region base address */
    uint32_t region_id = a->rn;
    TCGv_i64 src = cpu_reg(ctx, a->gpr);
    
    /* Map region_id to appropriate region struct */
    if (region_id < 2) {
        /* implicit_code[0-1] */
        tcg_gen_mov_i64(tcg_hfi.regions.implicit_code[region_id].reg_base_addr, src);
    } else if (region_id < 6) {
        /* implicit_data[0-3] */
        tcg_gen_mov_i64(tcg_hfi.regions.implicit_data[region_id - 2].reg_base_addr, src);
    } else if (region_id < 10) {
        /* explicit_data[0-3] */
        tcg_gen_mov_i64(tcg_hfi.regions.explicit_data[region_id - 6].reg_base_addr, src);
    }
    return true;
}

static bool trans_HFI_GRB(DisasContext* ctx, arg_HFI_GRB* a) {
    /* HFI Get Region Base - read region base address into gpr_dst */
    uint32_t region_id = a->rn;
    TCGv_i64 dst = cpu_reg(ctx, a->gpr);
    
    /* Map region_id to appropriate region struct */
    if (region_id < 2) {
        /* implicit_code[0-1] */
        tcg_gen_mov_i64(dst, tcg_hfi.regions.implicit_code[region_id].reg_base_addr);
    } else if (region_id < 6) {
        /* implicit_data[0-3] */
        tcg_gen_mov_i64(dst, tcg_hfi.regions.implicit_data[region_id - 2].reg_base_addr);
    } else if (region_id < 10) {
        /* explicit_data[0-3] */
        tcg_gen_mov_i64(dst, tcg_hfi.regions.explicit_data[region_id - 6].reg_base_addr);
    }
    return true;
}

static bool trans_HFI_SRM(DisasContext* ctx, arg_HFI_SRM* a) {
    /* HFI Set Region Mask - write gpr_src to region mask/bound */
    uint32_t region_id = a->rn;
    TCGv_i64 src = cpu_reg(ctx, a->gpr);
    
    /* Map region_id to appropriate region struct */
    if (region_id < 2) {
        /* implicit_code[0-1] - uses reg_lsb_mask */
        tcg_gen_mov_i64(tcg_hfi.regions.implicit_code[region_id].reg_lsb_mask, src);
    } else if (region_id < 6) {
        /* implicit_data[0-3] - uses reg_lsb_mask */
        tcg_gen_mov_i64(tcg_hfi.regions.implicit_data[region_id - 2].reg_lsb_mask, src);
    } else if (region_id < 10) {
        /* explicit_data[0-3] - uses reg_bound_addr */
        tcg_gen_mov_i64(tcg_hfi.regions.explicit_data[region_id - 6].reg_bound_addr, src);
    }
    return true;
}

static bool trans_HFI_GRM(DisasContext* ctx, arg_HFI_GRM* a) {
    /* HFI Get Region Mask - read region mask/bound into gpr_dst */
    uint32_t region_id = a->rn;
    TCGv_i64 dst = cpu_reg(ctx, a->gpr);
    
    /* Map region_id to appropriate region struct */
    if (region_id < 2) {
        /* implicit_code[0-1] - uses reg_lsb_mask */
        tcg_gen_mov_i64(dst, tcg_hfi.regions.implicit_code[region_id].reg_lsb_mask);
    } else if (region_id < 6) {
        /* implicit_data[0-3] - uses reg_lsb_mask */
        tcg_gen_mov_i64(dst, tcg_hfi.regions.implicit_data[region_id - 2].reg_lsb_mask);
    } else if (region_id < 10) {
        /* explicit_data[0-3] - uses reg_bound_addr */
        tcg_gen_mov_i64(dst, tcg_hfi.regions.explicit_data[region_id - 6].reg_bound_addr);
    }
    return true;
}

static bool trans_HFI_SRP(DisasContext* ctx, arg_HFI_SRP* a) {
    /* HFI Set Region Permissions - write gpr_src to region permissions */
    uint32_t region_id = a->rn;
    TCGv_i32 src = tcg_temp_new_i32();
    TCGv_i64 src64 = cpu_reg(ctx, a->gpr);
    
    /* Convert 64-bit source to 32-bit for permissions field */
    tcg_gen_extrl_i64_i32(src, src64);
    
    /* Map region_id to appropriate region struct */
    if (region_id < 2) {
        /* implicit_code[0-1] - has reg_perm_exec (bit 2) */
        tcg_gen_andi_i32(src, src, 0x4);  /* Mask to EXEC bit */
        tcg_gen_mov_i32(tcg_hfi.regions.implicit_code[region_id].reg_perm_exec, src);
    } else if (region_id < 6) {
        /* implicit_data[0-3] - has reg_perm_read and reg_perm_write (bits 0-1) */
        TCGv_i32 perm = tcg_temp_new_i32();
        tcg_gen_andi_i32(perm, src, 0x1);  /* READ bit (bit 0) */
        tcg_gen_mov_i32(tcg_hfi.regions.implicit_data[region_id - 2].reg_perm_read, perm);
        tcg_gen_andi_i32(perm, src, 0x2);  /* WRITE bit (bit 1) */
        tcg_gen_shri_i32(perm, perm, 1);
        tcg_gen_mov_i32(tcg_hfi.regions.implicit_data[region_id - 2].reg_perm_write, perm);
    } else if (region_id < 10) {
        /* explicit_data[0-3] - has reg_perm_read and reg_perm_write (bits 0-1) */
        TCGv_i32 perm = tcg_temp_new_i32();
        tcg_gen_andi_i32(perm, src, 0x1);  /* READ bit (bit 0) */
        tcg_gen_mov_i32(tcg_hfi.regions.explicit_data[region_id - 6].reg_perm_read, perm);
        tcg_gen_andi_i32(perm, src, 0x2);  /* WRITE bit (bit 1) */
        tcg_gen_shri_i32(perm, perm, 1);
        tcg_gen_mov_i32(tcg_hfi.regions.explicit_data[region_id - 6].reg_perm_write, perm);
    }
    return true;
}

static bool trans_HFI_GRP(DisasContext* ctx, arg_HFI_GRP* a) {
    /* HFI Get Region Permissions - read region permissions into gpr_dst */
    uint32_t region_id = a->rn;
    TCGv_i64 dst = cpu_reg(ctx, a->gpr);
    TCGv_i32 perm32 = tcg_temp_new_i32();
    TCGv_i64 perm64 = tcg_temp_new_i64();
    
    /* Map region_id to appropriate region struct */
    if (region_id < 2) {
        /* implicit_code[0-1] - read reg_perm_exec (bit 2) */
        tcg_gen_mov_i32(perm32, tcg_hfi.regions.implicit_code[region_id].reg_perm_exec);
        tcg_gen_extu_i32_i64(perm64, perm32);
        tcg_gen_mov_i64(dst, perm64);
    } else if (region_id < 6) {
        /* implicit_data[0-3] - read reg_perm_read and reg_perm_write */
        TCGv_i32 read_perm = tcg_temp_new_i32();
        TCGv_i32 write_perm = tcg_temp_new_i32();
        tcg_gen_mov_i32(read_perm, tcg_hfi.regions.implicit_data[region_id - 2].reg_perm_read);
        tcg_gen_mov_i32(write_perm, tcg_hfi.regions.implicit_data[region_id - 2].reg_perm_write);
        /* Reconstruct: read_perm in bit 0, write_perm in bit 1 */
        tcg_gen_shli_i32(write_perm, write_perm, 1);
        tcg_gen_or_i32(perm32, read_perm, write_perm);
        tcg_gen_extu_i32_i64(perm64, perm32);
        tcg_gen_mov_i64(dst, perm64);
    } else if (region_id < 10) {
        /* explicit_data[0-3] - read reg_perm_read and reg_perm_write */
        TCGv_i32 read_perm = tcg_temp_new_i32();
        TCGv_i32 write_perm = tcg_temp_new_i32();
        tcg_gen_mov_i32(read_perm, tcg_hfi.regions.explicit_data[region_id - 6].reg_perm_read);
        tcg_gen_mov_i32(write_perm, tcg_hfi.regions.explicit_data[region_id - 6].reg_perm_write);
        /* Reconstruct: read_perm in bit 0, write_perm in bit 1 */
        tcg_gen_shli_i32(write_perm, write_perm, 1);
        tcg_gen_or_i32(perm32, read_perm, write_perm);
        tcg_gen_extu_i32_i64(perm64, perm32);
        tcg_gen_mov_i64(dst, perm64);
    }
    return true;
}

static bool trans_HFI_SEH(DisasContext* ctx, arg_HFI_SEH* a) {
    /* HFI Set Exit Handler - write gpr to exit handler address */
    TCGv_i64 src = cpu_reg(ctx, a->gpr);
    TCGv_ptr handler_ptr = tcg_temp_new_ptr();
    
    /* Convert 64-bit register to pointer for handler address */
    tcg_gen_mov_i64((TCGv_i64)handler_ptr, src);
    tcg_gen_mov_ptr(tcg_hfi.exit_state.reg_exit_handler_addr, handler_ptr);
    
    return true;
}

static bool trans_HFI_GEH(DisasContext* ctx, arg_HFI_GEH* a) {
    /* HFI Get Exit Handler - read exit handler address into gpr */
    TCGv_i64 dst = cpu_reg(ctx, a->gpr);
    TCGv_ptr handler_ptr = tcg_temp_new_ptr();
    
    /* Convert pointer handler address to 64-bit register */
    tcg_gen_mov_ptr(handler_ptr, tcg_hfi.exit_state.reg_exit_handler_addr);
    tcg_gen_mov_i64(dst, (TCGv_i64)handler_ptr);
    
    return true;
}

static bool trans_HFI_ENTER(DisasContext* ctx, arg_HFI_ENTER* a) {
    /* HFI Enter Protected Region */
    nop(ctx);
    return true;
}

static bool trans_HFI_EXIT(DisasContext* ctx, arg_HFI_EXIT* a) {
    /* HFI Exit Protected Region */
    nop(ctx);
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
     * Writes to the HFI region registers hfi_implicit_region_base[region_number] and [region_number+1]
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
    int64_t region_number = a->region_number;

    /*
     * Write to HFI region base registers in CPUARMState
     */
    tcg_gen_mov_i64(tcg_hfi.regions.implicit_data[region_number].reg_base_addr, value1);
    tcg_gen_mov_i64(tcg_hfi.regions.implicit_data[region_number].reg_lsb_mask, value2);

    /* Temporary registers are freed automatically by TCG */
    return true;
}
