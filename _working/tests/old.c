#include <stdint.h>
#include <stdio.h>

#include "../hfi_test_helper.h"

int test_cstm() {
    int a = 0xDEADBEEF;
    // int a = 0x0;

    int b = 9/0;

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

int test_hfi_sr() {
    // placeholder
    struct {
        uint64_t reg_base_addr;
        uint64_t reg_lsb_mask;
    } region_desc;
    region_desc.reg_base_addr = 0x123456789ABC0000;
    region_desc.reg_lsb_mask = 0xFFFF;
    void* region_desc_ptr = &region_desc;

    asm volatile(
        "mov x0, %[region_desc_ptr]\n"
        "" HFI_SET_REGION(0, 0)  // HFI_SR with region_ptr_gpr=0 (using x0 as the source for the region descriptor pointer)
        :
        : [region_desc_ptr] "r"(region_desc_ptr)  // input
        : "x0"                                    // clobbered register
    );

    return 0;
}

TEST_MAIN({
    TEST(test_cstm(), 0xEEF ==);
    TEST(test_hfi_sr(), 0 ==);
})