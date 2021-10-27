#include "sbi.h"

extern char sbss[], ebss[];

void clear_bss() {
    for (char *p = sbss; p < ebss; p ++) {
        *p = 0;
    }
}

void kernel_main() {
    clear_bss();
    sbi_console_putchar('A');
    sbi_console_putchar('\n');
    sbi_shutdown();
}
