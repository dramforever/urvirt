#include "sbi.h"

#include <stdint.h>

#include "riscv-bits.h"

extern char sbss[], ebss[];

void clear_bss() {
    for (char *p = sbss; p < ebss; p ++) {
        *p = 0;
    }
}

const char str[] = "I wrote this with ecall from user mode\n";

__attribute__((naked))
void stvec() {
    asm(
        "addi sp, sp, -512\n\t"
        "sd t0, 0(sp)\n\t"
        "csrr t0, sstatus\n\t"
        "sd t0, 8(sp)\n\t"
        "csrr t0, sepc\n\t"
        "sd t0, 16(sp)\n\t"
        "sd a0, 24(sp)\n\t"
        "sd a7, 32(sp)\n\t"
        "addi a0, a0, 3\n\t"
        "li a7, 1\n\t"
        "ecall\n\t"
        "ld a0, 24(sp)\n\t"
        "ld a7, 32(sp)\n\t"
        "ld t0, 16(sp)\n\t"
        "addi t0, t0, 4\n\t"
        "csrw sepc, t0\n\t"
        "ld t0, 8(sp)\n\t"
        "csrw sstatus, t0\n\t"
        "ld t0, 0(sp)\n\t"
        "addi sp, sp, 512\n\t"
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
                : [data] "r"((*p) - 3)
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
    __builtin_unreachable();
}
