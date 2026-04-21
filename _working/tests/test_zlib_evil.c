#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hfi_test_helper.h"
#include "zlib.h"

// ============================================================================
// HFI Context State (from exit_handler_and_syscall.c)
// ============================================================================
uint64_t hfi_end_addr = -1;
uint64_t hfi_end_sp = -1;
uint64_t hfi_config = -1;

// Global variables to proxy arguments into the zlib sandbox
Bytef *dest_buf;
uLongf dest_len;
const Bytef *source_buf;
uLong source_len;
int zlib_result = -999; // Initialize to a dummy value

// ============================================================================
// Sandbox Proxy
// ============================================================================

// The proxy function that executes *inside* the HFI sandbox
void zlib_sandbox_proxy() {
    // Call the statically linked zlib function
    zlib_result = compress(dest_buf, &dest_len, source_buf, source_len);
    
    // Trigger QEMU to drop out of HFI mode entirely
    asm volatile(
        "" HFI_EXIT() 
        : : : "memory"
    );
}

// ============================================================================
// Assembly Scaffold & Handlers (from exit_handler_and_syscall.c)
// ============================================================================

void exit_handler_scaffold(void);
asm(
    ".text\n"
    ".align 4\n"
    ".global exit_handler_scaffold\n"
    ".type exit_handler_scaffold, @function\n"
    "exit_handler_scaffold:\n"

    // Save all registers to stack
    "sub sp, sp, #256\n"
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
    "mov x0, sp\n" // Pass stack pointer (regs array) as arg to exit_handler

    // Call the C exit handler
    "bl exit_handler\n"

    // Reconstruct PC
    "" HFI_GES(0) 
    "lsr x0, x0, #2\n"
    "lsl x0, x0, #2\n"
    "str x0, [sp, #128]\n" // Store temporarily in x16 slot

    // Restore all registers
    "ldp x0, x1, [sp, #0]\n"
    "ldp x2, x3, [sp, #16]\n"
    "ldp x4, x5, [sp, #32]\n"
    "ldp x6, x7, [sp, #48]\n"
    "ldp x8, x9, [sp, #64]\n"
    "ldp x10, x11, [sp, #80]\n"
    "ldp x12, x13, [sp, #96]\n"
    "ldp x14, x15, [sp, #112]\n"
    "ldp x16, x17, [sp, #128]\n" // x16 now has the target PC
    "ldp x18, x19, [sp, #144]\n"
    "ldp x20, x21, [sp, #160]\n"
    "ldp x22, x23, [sp, #176]\n"
    "ldp x24, x25, [sp, #192]\n"
    "ldp x26, x27, [sp, #208]\n"
    "ldp x28, x29, [sp, #224]\n"
    "ldr x30, [sp, #240]\n"
    "add sp, sp, #256\n" 

    // Advance PC past the svc instruction
    "add x16, x16, #4\n"
    
    // Load config and re-enter
    "adrp x17, hfi_config\n"
    "ldr x17, [x17, :lo12:hfi_config]\n"
    "" HFI_ENTER(17, 16) 
);

uint64_t do_syscall(uint64_t regs[31]) {
    printf("  -> Proxying syscall: x8=%lu, x0=%lu, x1=%lu, x2=%lu, x3=%lu, x4=%lu, x5=%lu\n", 
            regs[8], regs[0], regs[1], regs[2], regs[3], regs[4], regs[5]);
    uint64_t result;
    asm volatile(
        "mov x0, %[x0]\n"
        "mov x1, %[x1]\n"
        "mov x2, %[x2]\n"
        "mov x3, %[x3]\n"
        "mov x4, %[x4]\n"
        "mov x5, %[x5]\n"
        "mov x8, %[x8]\n"
        "svc #0\n"
        "str x0, %[result]\n"
        : [result] "=m"(result)
        : [x0] "r"(regs[0]), [x1] "r"(regs[1]), [x2] "r"(regs[2]), 
          [x3] "r"(regs[3]), [x4] "r"(regs[4]), [x5] "r"(regs[5]), [x8] "r"(regs[8])
        : "x6", "x7", "x16", "x17", "x30", "cc", "memory");
    return result;
}

void exit_handler(uint64_t regs[31]) {
    uint64_t exit_state = do_hfi_ges();
    uint64_t exit_reason = HFI_EXIT_STATE_GET_REASON(exit_state);

    if (exit_reason == HFI_SYSCALL_REQUESTED) {
        // Proxy the syscall and return to the scaffold so it can HFI_ENTER back into zlib
        uint64_t result = do_syscall(regs);
        regs[0] = result; 
    }
    else if (exit_reason == HFI_EXIT_CALLED) {
        // zlib finished executing and called HFI_EXIT(). Jump to the trusted return point.
        asm volatile(
            "mov sp, %[hfi_end_sp]\n"
            "br %[hfi_end_addr]\n"
            :
            : [hfi_end_addr] "r"(hfi_end_addr), [hfi_end_sp] "r"(hfi_end_sp)
            : "x0", "memory");
    }
}

// ============================================================================
// Test Logic
// ============================================================================

int is_true(int val) {
    return val == 1;
}

int test_zlib_evil() {
    // 1. Setup Data for compression
    const char* hello_msg = "Hello Hardware Fault Isolation! If you can read this, zlib sandboxing works while proxying syscalls.";
    source_len = strlen(hello_msg) + 1;
    source_buf = (const Bytef*)hello_msg;
    
    dest_len = compressBound(source_len);
    dest_buf = (Bytef*)malloc(dest_len);
    
    // 2. Configure HFI Regions
    do_hfi_srb(0, 0);
    do_hfi_srm(0, ~0ULL); // Global code mask for phase 1
    do_hfi_srp(0, HFI_PERM_EXEC | HFI_PERM_READ);

    do_hfi_srb(1, 0);
    do_hfi_srm(1, ~0ULL); // Global data mask for phase 1
    do_hfi_srp(1, HFI_PERM_READ | HFI_PERM_WRITE);

    // 3. Register Exit Handler
    do_hfi_seh((uint64_t)exit_handler_scaffold);
    hfi_config = HFI_OPT_LOCK_REGIONS;

    // 4. Enter the sandbox
    asm volatile(
        "mov x20, %[f]\n"
        "mov x21, %[hfi_config]\n"
        "adr x22, 1f\n"               // Address of the "1:" label below
        "str x22, %[hfi_end_addr]\n"  // Save return PC
        "mov x22, sp\n"
        "str x22, %[hfi_end_sp]\n"    // Save trusted stack pointer
        "" HFI_ENTER(21, 20)          // Jump into zlib_sandbox_proxy
        "1:\n"                        // The exit_handler branches here when done
        : [hfi_end_addr] "=m"(hfi_end_addr), [hfi_end_sp] "=m"(hfi_end_sp) 
        : [f] "r"(zlib_sandbox_proxy), [hfi_config] "r"(hfi_config)                         
        : "x20", "x21", "x22", "x29", "x30", "memory"
    );

    // 5. Verify Results upon return
    uint64_t fault_state = do_hfi_gfs();
    if (HFI_FAULT_STATE_GET_OCCURRED(fault_state)) {
        printf("  -> FAILED: A fault occurred inside zlib (Reason: %llu)\n", 
                HFI_FAULT_STATE_GET_REASON(fault_state));
        free(dest_buf);
        return 0;
    }

    if (zlib_result != Z_OK) {
        printf("  -> FAILED: zlib compress returned error %d\n", zlib_result);
        free(dest_buf);
        return 0;
    }

    printf("  -> SUCCESS: Proxied syscalls successfully. Compressed %lu bytes down to %lu bytes inside HFI.\n", 
            source_len, dest_len);
    free(dest_buf);
    return 1;
}

TEST_MAIN(
    TEST(test_zlib_evil(), is_true);
)