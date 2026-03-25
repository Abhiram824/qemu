
// =======================================================================
// =======================================================================
// MACROS for custom instructions
// =======================================================================
// =======================================================================

// =======================================================================
// helpers
// =======================================================================

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
