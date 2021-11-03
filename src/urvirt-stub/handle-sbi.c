#include <stdint.h>

#include "handle-sbi.h"
#include "urvirt_syscalls.h"

struct sbiret handle_sbi_call(
    uintptr_t which, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2) {

    if (which == SBI_CONSOLE_PUTCHAR) {
        char ch = arg0;
        s_write(1, (void *) &ch, 1);
        struct sbiret ret = { 0, 0 };
        return ret;
    } else if (which == SBI_SHUTDOWN) {
        write_log("SBI Shutdown!");
        s_exit_group(0);
        __builtin_unreachable();
    } else {
        struct sbiret ret = { SBI_ERR_NOT_SUPPORTED, 0 };
        return ret;
    }
}
