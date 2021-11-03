#include "sbi.h"

#include <stdint.h>

extern char sbss[], ebss[];

void clear_bss() {
    for (char *p = sbss; p < ebss; p ++) {
        *p = 0;
    }
}

const char str[] = "I wrote this with SBI calls\n";

void trace(uintptr_t value) {
    asm(
        "mv a0, %[src]\n\t"
        "li a7, 445\n\t"
        "ecall"
        :
        : [src] "r"(value)
        : "a0", "a7"
    );
}

void kernel_main() {
    clear_bss();

    uintptr_t x = 0x1234abcd98760123ull;

    asm(
        "csrw sscratch, %[data]"
        :
        : [data] "r"(x)
        :
    );

    uintptr_t a = 0xffff0000abcd1234ull;
    uintptr_t b;


    asm(
        "csrrw %[dest], sscratch, %[src]"
        : [dest] "=r"(b)
        : [src] "r"(a)
        :
    );

    uintptr_t c;

    asm(
        "csrr %[dest], sscratch"
        : [dest] "=r"(c)
        :
        :
    );

    trace(x);
    trace(a);
    trace(b);
    trace(c);


    for (const char *p = str; *p; p ++) {
        sbi_console_putchar(*p);
    }
    sbi_shutdown();
}
