#include <stdio.h>

int main(int argc, char const* argv[]) {
        
    int a = 0xDEADBEEF;
    // int a = 0x0;

    // write assembly to run add 1 to a, using register assembly
    __asm__ (
        "add x0, x0, 1\n"
        : "=r" (a) // output
        : "r" (a)  // input
    );

    // print the result
    printf("Result: 0x%X\n", a);

    return 0;
}
