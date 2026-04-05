#include <stdint.h>
#include <stdio.h>

#include "../hfi_test_helper.h"

uint64_t hfi_end_addr = -1;
uint64_t hfi_end_sp = -1;

void exit_handler() {
    printf("HI apples!\n");
    printf("addr: %lx, sp: %lx\n", hfi_end_addr, hfi_end_sp);

    // jump to hfi end address
    asm volatile(
        "mov sp, %[hfi_end_sp]\n"
        "br %[hfi_end_addr]\n"
        :
        : "r"(hfi_end_addr), [hfi_end_addr] "r"(hfi_end_addr), [hfi_end_sp] "r"(hfi_end_sp)
        : "x0");
}

void code_under_hfi() {
    uint64_t x = 9;
    x += 3;
    
    // make a syscall
    printf("Hello from inside the sandbox! Value of x: %lu\n", x);

    asm volatile(
        "mov x0, %0\n"
        "" HFI_EXIT()  // Exit HFI mode, should jump to exit handler
        :
        : "r"(x)
        : "x0");
}

int test_hfi_enter_exit() {
    void (*f)() = code_under_hfi;
    int options = HFI_OPT_LOCK_REGIONS;

    // get sandbox vars
    uint64_t code_region_base = (uint64_t)(f) & (~0xFFFFull);  // page-align the base
    uint64_t code_region_mask = ~0xFFFFull;
    uint64_t code_region_perms = HFI_PERM_EXEC;
    uint64_t exit_handler_addr = (uint64_t)exit_handler;

    // setup sandbox
    asm volatile(
        "mov x0, %0\n"    // Move region base to x0
        "" HFI_SRB(0, 0)  // Set region 0 with the above base
        "mov x1, %1\n"    // Move region mask to x1
        "" HFI_SRM(0, 1)  // Set region 0 mask
        "mov x2, %2\n"    // Move region permissions to x2
        "" HFI_SRP(0, 2)  // Set region 0 permissions
        "mov x3, %3\n"    // Move exit handler address to x3
        "" HFI_SEH(3)     // Set exit handler
        :
        : "r"(code_region_base), "r"(code_region_mask), "r"(code_region_perms), "r"(exit_handler_addr)
        : "x0", "x1", "x2", "x3");

    // enter the sandbox
    asm volatile(
        "mov x20, %[f]\n"        // Move function pointer to x20
        "mov x21, %[options]\n"  // Move options to x21

        // save trusted context
        "adr x22, 1f\n"
        "str x22, %[hfi_end_addr]\n"
        "mov x22, sp\n"
        "str x22, %[hfi_end_sp]\n"
        "" HFI_ENTER(21, 20)  // Enter HFI mode with region 1 locked (arbitrary choice for test)
        "1:\n"
        : [hfi_end_addr] "=m"(hfi_end_addr), [hfi_end_sp] "=m"(hfi_end_sp)  // Output operand
        : [f] "r"(f), [options] "r"(options)                                // Input operands
        : "x29", "x30" 
    );

    return 5;
}

TEST_MAIN({
    TEST(test_hfi_enter_exit(), 5 ==);
});