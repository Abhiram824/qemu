#include <stdio.h>

#include "../hfi_test_helper.h"

int test_cstm() {
    int a = 0xDEADBEEF;
    // int a = 0x0;

    // write assembly to run add 1 to a, using register assembly
    asm volatile(
        "mov x0, %[a]\n"
        "" CSTM(0, 0)  // CSTM with rn=0 and rd=0 (using a as both source and destination)
        "mov %[a], x0\n"
        : [a] "+r"(a)  // input/output
        :
        : "x0"  // clobbered register
    );

    return a;
}

TEST_MAIN({
    TEST(test_cstm(), 0xEEF ==);
})