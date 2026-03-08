
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
