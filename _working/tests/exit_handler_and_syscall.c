#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../hfi_test_helper.h"

uint64_t hfi_end_addr = -1;
uint64_t hfi_end_sp = -1;
uint64_t hfi_config = -1;

void exit_handler_scaffold(void);
asm (
    ".text\n"
    ".align 4\n"
    ".global exit_handler_scaffold\n"
    ".type exit_handler_scaffold, @function\n"
    "exit_handler_scaffold:\n"
    
    // Save all registers to stack (we'll restore later)
    "sub sp, sp, #256\n"           // Allocate space for 32 x64 regs
    "stp x0, x1, [sp, #0]\n"
    "stp x2, x3, [sp, #16]\n"
    "stp x4, x5, [sp, #32]\n"
    "stp x6, x7, [sp, #48]\n"
    "stp x8, x9, [sp, #64]\n"
    "stp x10, x11, [sp, #80]\n"
    "stp x12, x13, [sp, #96]\n"
    "stp x14, x15, [sp, #112]\n"
    "stp x16, x17, [sp, #128]\n"
    "stp x18, x19, [sp, #144]\n"
    "stp x20, x21, [sp, #160]\n"
    "stp x22, x23, [sp, #176]\n"
    "stp x24, x25, [sp, #192]\n"
    "stp x26, x27, [sp, #208]\n"
    "stp x28, x29, [sp, #224]\n"
    "str x30, [sp, #240]\n"
    
    // Call the C exit handler - returns exit_state in x0
    "bl exit_handler\n"
    
    // get PC from exit state (bits[63:2]), reconstruct full PC by shifting, and store temporarily
    "" HFI_GES(0)  // Get exit state into x0

    // Extract PC: shift right by 2 to get offset from bits[63:2], then shift left by 2
    "lsr x0, x0, #2\n"
    "lsl x0, x0, #2\n"
    // Store the target PC temporarily in x16 location on stack (will restore it as x16 value)
    "str x0, [sp, #128]\n"
    
    // Restore all registers
    "ldp x0, x1, [sp, #0]\n"
    "ldp x2, x3, [sp, #16]\n"
    "ldp x4, x5, [sp, #32]\n"
    "ldp x6, x7, [sp, #48]\n"
    "ldp x8, x9, [sp, #64]\n"
    "ldp x10, x11, [sp, #80]\n"
    "ldp x12, x13, [sp, #96]\n"
    "ldp x14, x15, [sp, #112]\n"
    "ldp x16, x17, [sp, #128]\n"   // x16 now has the target PC
    "ldp x18, x19, [sp, #144]\n"
    "ldp x20, x21, [sp, #160]\n"
    "ldp x22, x23, [sp, #176]\n"
    "ldp x24, x25, [sp, #192]\n"
    "ldp x26, x27, [sp, #208]\n"
    "ldp x28, x29, [sp, #224]\n"
    "ldr x30, [sp, #240]\n"
    "add sp, sp, #256\n"           // Deallocate stack space
    
    // Jump back to the saved PC (x16 has the target PC from exit state)
    // add 4 to skip the instruction that caused the exit (the svc)'
    "add x16, x16, #4\n"
    // load the options for the jump from the global variable (hfi_config)
    "adrp x17, hfi_config\n"
    "ldr x17, [x17, :lo12:hfi_config]\n"
    "" HFI_ENTER(17, 16)  // Re-enter HFI mode with options in x17, target in x16
);

void exit_handler(void) {
    printf("[EXIT_HANDLER] Exit handler called!\n");

    // check if its a syscall
    uint64_t exit_state = do_hfi_ges();
    uint64_t exit_reason = HFI_EXIT_STATE_GET_REASON(exit_state);
    uint64_t exit_pc = HFI_EXIT_STATE_GET_PC(exit_state);
    
    if (exit_reason == HFI_SYSCALL_REQUESTED) {
        printf("Syscall requested at PC: 0x%llx\n", (unsigned long long)exit_pc);
    } else if (exit_reason == HFI_EXIT_CALLED) {
        printf("HFI exit called at PC: 0x%llx\n", (unsigned long long)exit_pc);
        asm volatile(
            "mov sp, %[hfi_end_sp]\n"
            "br %[hfi_end_addr]\n"
            :
            : [hfi_end_addr] "r"(hfi_end_addr), [hfi_end_sp] "r"(hfi_end_sp)
            : "x0");
    }
}

void code_under_hfi() {
    uint64_t x = 9;
    x += 3;

    // make a syscall
    printf("[INSIDE HFI] Hello from inside the sandbox! Value of x: %lu\n", x);

    asm volatile(
        "mov x0, %0\n"
        "" HFI_EXIT()  // Exit HFI mode, should jump to exit handler
        :
        : "r"(x)
        : "x0");
}

int test_hfi_enter_exit(void) {
    void (*f)(void) = code_under_hfi;
    hfi_config = HFI_OPT_LOCK_REGIONS;

    // get sandbox vars
    uint64_t code_region_base = (uint64_t)(f) & (~0xFFFFull);  // page-align the base
    uint64_t code_region_mask = ~0xFFFFull;
    uint64_t code_region_perms = HFI_PERM_EXEC;
    uint64_t exit_handler_addr = (uint64_t)exit_handler_scaffold;

    do_hfi_srb(0, code_region_base);
    do_hfi_srm(0, code_region_mask);
    do_hfi_srp(0, code_region_perms);
    do_hfi_seh(exit_handler_addr);

    printf("Exit handler set to scaffold at address: 0x%llx\n", (unsigned long long)exit_handler_addr);
    printf("Entering HFI sandbox...\n");

    // enter the sandbox
    asm volatile(
        "mov x20, %[f]\n"        // Move function pointer to x20
        "mov x21, %[hfi_config]\n"  // Move hfi_config to x21

        // save trusted context
        "adr x22, 1f\n"
        "str x22, %[hfi_end_addr]\n"
        "mov x22, sp\n"
        "str x22, %[hfi_end_sp]\n"
        "" HFI_ENTER(21, 20)  // Enter HFI mode with region 1 locked (arbitrary choice for test)
        "1:\n"
        : [hfi_end_addr] "=m"(hfi_end_addr), [hfi_end_sp] "=m"(hfi_end_sp)  // Output operand
        : [f] "r"(f), [hfi_config] "r"(hfi_config)                                // Input operands
        : "x29", "x30" 
    );

    return 5;
}

TEST_MAIN({
    TEST(test_hfi_enter_exit(), 5 ==);
});