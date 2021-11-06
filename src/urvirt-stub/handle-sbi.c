#include <stdint.h>

#include "riscv-bits.h"
#include "handle-sbi.h"
#include "urvirt-syscalls.h"

struct sbiret handle_sbi_call(
    struct priv_state *priv,
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
    } else if (which == SBI_SET_TIMER) {
        uintptr_t cur_time;

        asm("csrr %0, time" : "=r"(cur_time) : : );
        uintptr_t delta_ns = (arg0 - cur_time) * 1000;

        write_log("Set timer");

        struct itimerspec spec;
        spec.it_interval.tv_sec = 0;
        spec.it_interval.tv_nsec = 0;
        spec.it_value.tv_sec = delta_ns / 1000000000ul;
        spec.it_value.tv_nsec = delta_ns % 1000000000ul;
        s_timer_settime(priv->timerid, 0, &spec, NULL);

        priv->sip = set_six_sti(priv->sip, arg0 <= cur_time);
    } else {
        write_log("Unhandled sbi call");
        struct sbiret ret = { SBI_ERR_NOT_SUPPORTED, 0 };
        return ret;
    }
}
