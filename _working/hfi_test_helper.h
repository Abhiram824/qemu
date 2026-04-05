

// ===============================================================================
// ================================= HFI CONSTS ==================================
// ===============================================================================

#define HFI_OPT_LOCK_REGIONS 0x1

/* HFI_FaultReason */
#define HFI_FAULT_OUT_OF_BOUNDS 1
#define HFI_FAULT_PERMISSION 2

/* HFI_FaultOperation */
#define HFI_FAULT_OPERATION_LOAD 1
#define HFI_FAULT_OPERATION_STORE 2
#define HFI_FAULT_OPERATION_FETCH 3

/* HFI_ExitReason */
#define HFI_EXIT_CALLED 1
#define HFI_SYSCALL_REQUESTED 2

/* HFI_Permissions */
#define HFI_PERM_READ 0x1
#define HFI_PERM_WRITE 0x2
#define HFI_PERM_EXEC 0x4
#define HFI_PERM_ALL_MASK (HFI_PERM_READ | HFI_PERM_WRITE | HFI_PERM_EXEC)

/* HFI_Flags */
#define HFI_REGION_IS_LARGE 0x8

// =======================================================================
// =======================================================================
// MACROS for custom instructions
// =======================================================================
// =======================================================================

// =======================================================================
// helpers
// =======================================================================

#include "stdio.h"

#define _TO_STR(x) #x

#define _TO_ASM_INSTR_STR(x) \
    "/* \t" #x               \
    " \t*/\t\t\t"            \
    ".inst" _TO_STR(x) "\n"

// =======================================================================
// value generator macros
// =======================================================================

#define CSTM_VAL(rn, rd) \
    (0xE7FFFC00 | ((rn & 0x1F) << 5) | (rd & 0x1F))

#define HFI_SET_REGION_VAL(region_number, region_ptr_gpr) \
    (0xE7C00000 | ((region_number & 0x7) << 5) | (region_ptr_gpr & 0x1F))

// =======================================================================
// actual instruction macros
// =======================================================================

#define CSTM(rn, rd) \
    _TO_ASM_INSTR_STR(CSTM_VAL(rn, rd))

#define HFI_SET_REGION(region_number, region_ptr_gpr) \
    _TO_ASM_INSTR_STR(HFI_SET_REGION_VAL(region_number, region_ptr_gpr))

/* HFI instruction value generators (from decoder spec) */
/* Convention: Destination register/region is always first */
/* For SET ops: region_id (destination) is param 1, gpr_src (source) is param 2 */
/* For GET ops: gpr_dst (destination) is param 1, region_id (source) is param 2 */
/* All HFI instructions start with 0x02xxxxxx base opcode */

/* HFI_SRB <region_id> <gpr_src>: Set Region Base - write from gpr_src to region_id */
#define HFI_SRB_VAL(region_id, gpr_src) \
    (0x02000000 | ((region_id & 0x0F) << 5) | (gpr_src & 0x1F))

/* HFI_GRB <gpr_dst> <region_id>: Get Region Base - read region_id into gpr_dst */
#define HFI_GRB_VAL(gpr_dst, region_id) \
    (0x02010000 | ((region_id & 0x0F) << 5) | (gpr_dst & 0x1F))

/* HFI_SRM <region_id> <gpr_src>: Set Region Mask - write from gpr_src to region_id */
#define HFI_SRM_VAL(region_id, gpr_src) \
    (0x02020000 | ((region_id & 0x0F) << 5) | (gpr_src & 0x1F))

/* HFI_GRM <gpr_dst> <region_id>: Get Region Mask - read region_id into gpr_dst */
#define HFI_GRM_VAL(gpr_dst, region_id) \
    (0x02030000 | ((region_id & 0x0F) << 5) | (gpr_dst & 0x1F))

/* HFI_SRP <region_id> <gpr_src>: Set Region Permissions - write from gpr_src to region_id */
#define HFI_SRP_VAL(region_id, gpr_src) \
    (0x02040000 | ((region_id & 0x0F) << 5) | (gpr_src & 0x1F))

/* HFI_GRP <gpr_dst> <region_id>: Get Region Permissions - read region_id into gpr_dst */
#define HFI_GRP_VAL(gpr_dst, region_id) \
    (0x02050000 | ((region_id & 0x0F) << 5) | (gpr_dst & 0x1F))

#define HFI_SEH_VAL(gpr) \
    (0x02060000 | (gpr & 0x1F))

#define HFI_GEH_VAL(gpr) \
    (0x02070000 | (gpr & 0x1F))

#define HFI_ENTER_VAL(optr, gpr) \
    (0x02080000 | ((optr & 0x1F) << 5) | (gpr & 0x1F))

#define HFI_EXIT_VAL() \
    (0x02090000)

/* HFI_GFS <gpr>: Get Fault State - read fault state into gpr */
#define HFI_GFS_VAL(gpr) \
    (0x020A0000 | (gpr & 0x1F))

/* HFI_SFS <gpr>: Set Fault State - write from gpr to fault state */
#define HFI_SFS_VAL(gpr) \
    (0x020B0000 | (gpr & 0x1F))

/* HFI_GES <gpr>: Get Exit State - read exit state into gpr */
#define HFI_GES_VAL(gpr) \
    (0x020C0000 | (gpr & 0x1F))

/* HFI_SES <gpr>: Set Exit State - write from gpr to exit state */
#define HFI_SES_VAL(gpr) \
    (0x020D0000 | (gpr & 0x1F))

/* HFI instruction macros */
#define HFI_SRB(region_id, gpr_src) \
    _TO_ASM_INSTR_STR(HFI_SRB_VAL(region_id, gpr_src))

#define HFI_GRB(gpr_dst, region_id) \
    _TO_ASM_INSTR_STR(HFI_GRB_VAL(gpr_dst, region_id))

#define HFI_SRM(region_id, gpr_src) \
    _TO_ASM_INSTR_STR(HFI_SRM_VAL(region_id, gpr_src))

#define HFI_GRM(gpr_dst, region_id) \
    _TO_ASM_INSTR_STR(HFI_GRM_VAL(gpr_dst, region_id))

#define HFI_SRP(region_id, gpr_src) \
    _TO_ASM_INSTR_STR(HFI_SRP_VAL(region_id, gpr_src))

#define HFI_GRP(gpr_dst, region_id) \
    _TO_ASM_INSTR_STR(HFI_GRP_VAL(gpr_dst, region_id))

#define HFI_SEH(gpr) \
    _TO_ASM_INSTR_STR(HFI_SEH_VAL(gpr))

#define HFI_GEH(gpr) \
    _TO_ASM_INSTR_STR(HFI_GEH_VAL(gpr))

#define HFI_ENTER(optr, gpr) \
    _TO_ASM_INSTR_STR(HFI_ENTER_VAL(optr, gpr))

#define HFI_EXIT() \
    _TO_ASM_INSTR_STR(HFI_EXIT_VAL())

#define HFI_GFS(gpr) \
    _TO_ASM_INSTR_STR(HFI_GFS_VAL(gpr))

#define HFI_SFS(gpr) \
    _TO_ASM_INSTR_STR(HFI_SFS_VAL(gpr))

#define HFI_GES(gpr) \
    _TO_ASM_INSTR_STR(HFI_GES_VAL(gpr))

#define HFI_SES(gpr) \
    _TO_ASM_INSTR_STR(HFI_SES_VAL(gpr))

// =======================================================================
// helper functions (inlined)
// =======================================================================

inline void do_hfi_srb(uint32_t region_id, uint64_t value) {
    asm volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "" HFI_SRB(0, 0)  // Set region base
        :
        : "r"(region_id), "r"(value)
        : "x0", "x1");
}

inline uint64_t do_hfi_grb(uint32_t region_id) {
    uint64_t result;
    asm volatile(
        "mov x0, %0\n"
        "" HFI_GRB(0, 0)  // Get region base into x0
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(region_id)
        : "x0");
    return result;
}

inline void do_hfi_srm(uint32_t region_id, uint64_t value) {
    asm volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "" HFI_SRM(0, 0)  // Set region mask
        :
        : "r"(region_id), "r"(value)
        : "x0", "x1");
}

inline uint64_t do_hfi_grm(uint32_t region_id) {
    uint64_t result;
    asm volatile(
        "mov x0, %0\n"
        "" HFI_GRM(0, 0)  // Get region mask into x0
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(region_id)
        : "x0");
    return result;
}

inline void do_hfi_srp(uint32_t region_id, uint64_t value) {
    asm volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "" HFI_SRP(0, 0)  // Set region permissions
        :
        : "r"(region_id), "r"(value)
        : "x0", "x1");
}

inline uint64_t do_hfi_grp(uint32_t region_id) {
    uint64_t result;
    asm volatile(
        "mov x0, %0\n"
        "" HFI_GRP(0, 0)  // Get region permissions into x0
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(region_id)
        : "x0");
    return result;
}

inline void do_hfi_seh(uint64_t value) {
    asm volatile(
        "mov x0, %0\n"
        "" HFI_SEH(0)  // Set exit handler from x0
        :
        : "r"(value)
        : "x0");
}

inline uint64_t do_hfi_geh(void) {
    uint64_t result;
    asm volatile(
        "mov x0, 0\n"
        "" HFI_GEH(0)  // Get exit handler into x0
        "mov %0, x0\n"
        : "=r"(result)
        :
        : "x0");
    return result;
}

inline void do_hfi_enter(uint64_t jump_target, uint64_t options) __attribute__((noreturn)) {
    asm volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "" HFI_ENTER(1, 0)  // Enter with options in x1, target in x0
        :
        : "r"(jump_target), "r"(options)
        : "x0", "x1");
}

inline void do_hfi_exit(void) {
    asm volatile(
        "" HFI_EXIT()  // Exit protected region
        :
        :
        :);
}

inline uint64_t do_hfi_gfs(void) {
    uint64_t result;
    asm volatile(
        "mov x0, 0\n"
        "" HFI_GFS(0)  // Get fault state into x0
        "mov %0, x0\n"
        : "=r"(result)
        :
        : "x0");
    return result;
}

inline void do_hfi_sfs(uint64_t value) {
    asm volatile(
        "mov x0, %0\n"
        "" HFI_SFS(0)  // Set fault state from x0
        :
        : "r"(value)
        : "x0");
}

inline uint64_t do_hfi_ges(void) {
    uint64_t result;
    asm volatile(
        "mov x0, 0\n"
        "" HFI_GES(0)  // Get exit state into x0
        "mov %0, x0\n"
        : "=r"(result)
        :
        : "x0");
    return result;
}

inline void do_hfi_ses(uint64_t value) {
    asm volatile(
        "mov x0, %0\n"
        "" HFI_SES(0)  // Set exit state from x0
        :
        : "r"(value)
        : "x0");
}

// =======================================================================
// testing orchestration macros
// =======================================================================

#define SETUP_TEST_SUITE() int passed = 0, failed = 0

#define TEST(test_call, expected_check)              \
    do {                                             \
        printf("Running test: %s\n", #test_call);    \
        if (expected_check(test_call)) {             \
            printf("Test passed: %s\n", #test_call); \
            passed++;                                \
        } else {                                     \
            printf("Test failed: %s\n", #test_call); \
            failed++;                                \
        }                                            \
    } while (0)

#define END_TEST_SUITE() return failed

#define TEST_MAIN(tests)                     \
    int main(int argc, char const* argv[]) { \
        SETUP_TEST_SUITE();                  \
        tests                                \
        END_TEST_SUITE();                    \
    }
