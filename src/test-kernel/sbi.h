#pragma once

#include <stdint.h>
#include <stddef.h>

const size_t SBI_SET_TIMER = 0;
const size_t SBI_CONSOLE_PUTCHAR = 1;
const size_t SBI_CONSOLE_GETCHAR = 2;
const size_t SBI_SHUTDOWN = 8;

static inline uintptr_t sbi_call(size_t which, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2) {
    register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1);
    register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2);
    register uintptr_t a7 asm ("a7") = (uintptr_t)(which);
    asm volatile ("wfi"
                    : "+r" (a0)
                    : "r" (a1), "r" (a2), "r" (a7)
                    : "memory");
    return a0;
}


static inline void sbi_console_putchar(size_t c) {
    sbi_call(SBI_CONSOLE_PUTCHAR, c, 0, 0);
}

static inline size_t sbi_console_getchar() {
    sbi_call(SBI_CONSOLE_GETCHAR, 0, 0, 0);
}

static inline void sbi_shutdown() {
    sbi_call(SBI_SHUTDOWN, 0, 0, 0);
    for(;;) {}
}
