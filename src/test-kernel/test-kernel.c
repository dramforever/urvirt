#include "sbi.h"

#include <stdint.h>

#include "riscv-bits.h"

extern char sbss[], ebss[];

void clear_bss() {
    for (char *p = sbss; p < ebss; p ++) {
        *p = 0;
    }
}

const char str[] = "I wrote this with SBI calls\n";

__attribute__((naked))
void stvec() {
    asm(
        "addi a0, a0, 1\n\t"
        "li a7, 1\n\t"
        "ecall\n\t"
        "sret\n\t"
    );
}

void umode() {
    while (1) {
        for (const char *p = str; *p; p ++) {
            asm(
                "mv a0, %[data]\n\t"
                "ecall\n\t"
                :
                : [data] "r"((*p) - 1)
                :
            );
        }
    }
}

void kernel_main() {
    asm(
        "csrc sstatus, %[value]"
        :
        : [value] "r"(set_sstatus_spp(0, MASK_sstatus_spp))
        :
    );

    asm(
        "csrw stvec, %[value]"
        :
        : [value] "r"(stvec)
        :
    );

    asm(
        "csrw sepc, %[value]"
        :
        : [value] "r"(umode)
        :
    );

    asm("sret");
}
