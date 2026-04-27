#include "../hfi_test_helper.h"
#include "../hfi_memory_test_helpers.h"


// copied from  qemu/_working/tests/exit_handler_and_syscall.c
uint64_t hfi_end_addr = -1;
uint64_t hfi_end_sp = -1;
uint64_t hfi_config = -1;

static int g_fault_occurred = 0;

void exit_handler_scaffold(void);
asm(
    ".text\n"
    ".align 4\n"
    ".global exit_handler_scaffold\n"
    ".type exit_handler_scaffold, @function\n"
    "exit_handler_scaffold:\n"

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
    "mov x0, sp\n"

    "bl exit_handler\n"

    // Syscall-continuation path: re-enter HFI at (exit_pc + 4)
    "" HFI_GES(0)
    "lsr x0, x0, #2\n"
    "lsl x0, x0, #2\n"
    "str x0, [sp, #128]\n"

    "ldp x0, x1, [sp, #0]\n"
    "ldp x2, x3, [sp, #16]\n"
    "ldp x4, x5, [sp, #32]\n"
    "ldp x6, x7, [sp, #48]\n"
    "ldp x8, x9, [sp, #64]\n"
    "ldp x10, x11, [sp, #80]\n"
    "ldp x12, x13, [sp, #96]\n"
    "ldp x14, x15, [sp, #112]\n"
    "ldp x16, x17, [sp, #128]\n"
    "ldp x18, x19, [sp, #144]\n"
    "ldp x20, x21, [sp, #160]\n"
    "ldp x22, x23, [sp, #176]\n"
    "ldp x24, x25, [sp, #192]\n"
    "ldp x26, x27, [sp, #208]\n"
    "ldp x28, x29, [sp, #224]\n"
    "ldr x30, [sp, #240]\n"
    "add sp, sp, #256\n"

    "add x16, x16, #4\n"
    "adrp x17, hfi_config\n"
    "ldr x17, [x17, :lo12:hfi_config]\n"
    "" HFI_ENTER(17, 16)
);

void exit_handler(uint64_t regs[31])
{
    uint64_t fs = do_hfi_gfs();
    g_fault_occurred = (HFI_FAULT_STATE_GET_OCCURRED(fs) != 0);

    if (g_fault_occurred) {
        printf("HFI fault caught (OOB or permission violation).\n");
    } else {
        printf("Sandbox exited cleanly via HFI_EXIT.\n");
    }
    fflush(stdout);

    asm volatile(
        "mov sp, %[hfi_end_sp]\n"
        "br  %[hfi_end_addr]\n"
        :
        : [hfi_end_addr] "r"(hfi_end_addr), [hfi_end_sp] "r"(hfi_end_sp)
        :);
}

void test_hfi(int store_inside) {
    uint32_t len = 128;
    void *code = alloc_code(4096);
    void *data = alloc_data(4096);

    HFI_SRB_X1(code, 7);
    HFI_SRM_X1(len, 7);
    HFI_SRP_X1(0x5, 7);

    // configure region 2 (explicit data)
    HFI_SRB_X1(data, 6);
    HFI_SRM_X1(len, 6);
    HFI_SRP_X1(0x3, 6);   // READ | WRITE

    uint32_t *insns = code;
    uint32_t insn1 = store_inside ? HSTRBU_I_INSTR_VAL(0,4,2) : HSTRBU_I_INSTR_VAL(0,4,252);
    insns[0] = insn1;
    insns[1] = HFI_EXIT_VAL();  // HFI_EXIT

    void (*scaffold)() = exit_handler_scaffold;
    asm volatile(
        "mov x3, %[ret]\n"
        "" HFI_SEH(3)
        :
        : [ret] "r"(scaffold)
        : "x3");

    hfi_config = 0x1;   // HFI_OPT_LOCK_REGIONS
    asm volatile(
        "adr  x22, 1f\n"
        "str  x22, %[end_addr]\n"
        "mov  x22, sp\n"
        "str  x22, %[end_sp]\n"
        "mov  x4,  %[data]\n"
        "mov  x1,  %[code]\n"
        "mov  x2,  %[cfg]\n"
        "" HFI_ENTER(2, 1)
        "1:\n"
        : [end_addr] "=m"(hfi_end_addr), [end_sp] "=m"(hfi_end_sp)
        : [code] "r"(code), [data] "r"(data), [cfg] "r"(hfi_config)
        : "x1", "x2", "x4", "x22", "x29", "x30");
}

int main() {
    printf("Testing HFI memory access with store inside region...\n");
    fflush(stdout);
    test_hfi(1);
    if (g_fault_occurred) {
        printf("TEST FAILED: Unexpected fault on in-bounds access!\n");
        return 1;
    }
    printf("Success!\n");
    fflush(stdout);

    printf("Testing HFI memory access with store outside region...\n");
    fflush(stdout);
    test_hfi(0);
    if (!g_fault_occurred) {
        printf("TEST FAIL: no fault raised despite doing oob access!\n");
        return 1;
    }
    printf("Success! Out-of-bounds access was correctly caught.\n");
    fflush(stdout);

    return 0;
}
