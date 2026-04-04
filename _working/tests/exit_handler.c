#include <stdint.h>
#include <stdio.h>

#include "../hfi_test_helper.h"

uint64_t hfi_end_addr = -1;
uint64_t hfi_end_sp = -1;

void exit_handler() {
    printf("HI apples!\n");
    
    // jump to hfi end address 
    asm volatile(
        "mov x0, %0\n"
        "br x0\n"
        :
        : "r"(hfi_end_addr)
        : "x0");
}

void code_under_hfi() {
    uint64_t x = 9;
    x += 3;
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
    uint64_t ret = 0;

    // get sandbox vars
    uint64_t code_region_base = (uint64_t)(f) & (~0xFFFFull);  // page-align the base
    uint64_t code_region_mask = ~0xFFFFull;
    uint64_t code_region_perms = HFI_PERM_EXEC;
    uint64_t exit_handler_addr = (uint64_t)exit_handler;

    
    // setup sandbox
    asm volatile(
        "mov x0, %0\n"    // Move region base to x0
        "mov x1, %1\n"    // Move region mask to x1
        "mov x2, %2\n"    // Move region permissions to x2
        "mov x3, %3\n"    // Move exit handler address to x3
        "" HFI_SRB(0, 0)  // Set region 0 with the above base
        "" HFI_SRM(0, 1)  // Set region 0 mask
        "" HFI_SRP(0, 2)  // Set region 0 permissions
        "" HFI_SEH(3)     // Set exit handler
        :
        : "r"(code_region_base), "r"(code_region_mask), "r"(code_region_perms), "r"(exit_handler_addr)
        : "x0", "x1", "x2", "x3");
    
    // save hfi context
    hfi_end_addr = (uint64_t)&&hfi_end;  // Set the end address for the exit handler to jump to
    uint64_t sp;
    asm volatile("mov %0, sp" : "=r"(sp));
    hfi_end_sp = sp;  // Save the stack pointer to restore it in the exit handler
    printf("Saved HFI end address: 0x%lx, end SP: 0x%lx\n", hfi_end_addr, hfi_end_sp);

    // enter the sandbox
    asm volatile(
        "mov x0, %1\n"          // Move function pointer to x0
        "mov x1, %2\n"          // Move options to x1
        "" HFI_ENTER(1, 0)      // Enter HFI mode with region 1 locked (arbitrary choice for test)
        : "=r"(ret)             // Output operand
        : "r"(f), "r"(options)  // Input operands
        : "x0", "x1"            // Clobbered registers
    );

    // exit point, set sp too
    hfi_end:
    asm volatile("mov sp, %0" : : "r"(hfi_end_sp));  // Restore the stack pointer

    // get sp... this is really scuffed rn
    uint64_t final_sp;
    asm volatile("mov %0, sp" : "=r"(final_sp));
    printf("Final SP after exit: 0x%lx\n", final_sp);

    return 5;
}

TEST_MAIN({
    TEST(test_hfi_enter_exit(), 5 ==);
});