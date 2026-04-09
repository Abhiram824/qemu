#include <sys/mman.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>


#include "../hfi_test_helper.h"

void hfi_return() {
    printf("Returned from HFI sandbox successfully!\n");
    fflush(stdout);
}

void test_hfi(int load_inside) {
    uint32_t len = 128;
    void *code = mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    void *data = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    // configure region 0 (implicit code) to cover the code region
    asm volatile(
        "mov x1, %[code]\n"
        "" HFI_SRB(0, 1)
        :
        : [code] "r"(code)
        : "x1");
    
    asm volatile(
        "mov x1, %[len]\n"
        "" HFI_SRM(0, 1)
        :
        : [len] "r"(len)
        : "x1");
    
    asm volatile(
        "mov x1, #0x5\n"  // EXEC | READ permissions
        "" HFI_SRP(0, 1)
        );
    
    // configure region 2 (implicit data) to cover the data region
    asm volatile(
        "mov x1, %[data]\n"
        "" HFI_SRB(2, 1)
        :
        : [data] "r"(data)
        : "x1");
    
    asm volatile(
        "mov x1, %[len]\n"
        "" HFI_SRM(2, 1)
        :
        : [len] "r"(len)
        : "x1");
    
    asm volatile(
        "mov x1, #0x3\n"  // EXEC | READ permissions
        "" HFI_SRP(2, 1)
        );
    

    // write LDR + HFI_EXIT into code region
    uint32_t *insns = code;
    uint32_t insn1 = load_inside ? 0b00111000010000000100000000100001 : 0b00111000010011111111000000100001;   // LDRi #0x2 x1, [x1]
    insns[0] = insn1; 
    insns[1] = 0b00000010000010010000000000000000;  // HFI_EXIT

    void (*hfi_exit)() = hfi_return;
    // set exit handler
    asm volatile(
        "mov x3, %[ret]\n"
        "" HFI_SEH(3)
        :
        : [ret] "r"(hfi_exit)
        : "x3");

    // enter the sandbox — HFI_EXIT will jump to hfi_return
    // ldr x30, [x29, #8]: restore LR from the saved frame record so that
    // hfi_return's "ret" goes back to main (not a stale value from the last bl)
    asm volatile(
        "ldr x30, [x29, #8]\n"
        "mov x1, %[code]\n"
        "mov x2, #0x1\n"
        "" HFI_ENTER(1, 2)
        :
        : [code] "r"(code)
        : "x1", "x2", "x30");

}

int main() {
    printf("Testing HFI memory access with load inside region...\n");
    fflush(stdout);
    test_hfi(1);
    printf("Success!\n");
    fflush(stdout);

    printf("Testing HFI memory access with load outside region...\n");
    fflush(stdout);
    test_hfi(0);
    printf("If we got here, the test failed to raise an exception on out-of-bounds access!\n");

    return 0;
}
