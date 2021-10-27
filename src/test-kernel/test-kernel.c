#include "sbi.h"

extern char sbss[], ebss[];

void clear_bss() {
    for (char *p = sbss; p < ebss; p ++) {
        *p = 0;
    }
}

const char str[] = "I wrote this with SBI calls\n";

void kernel_main() {
    clear_bss();
    for (const char *p = str; *p; p ++) {
        sbi_console_putchar(*p);
    }
    sbi_shutdown();
}
