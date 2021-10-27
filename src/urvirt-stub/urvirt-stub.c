#include <signal.h>
#include <ucontext.h>
#include <syscall.h>

#include "common.h"
#include "urvirt_syscalls.h"

void handler(int sig, siginfo_t *info, void *ucontext_voidp) {
    ucontext_t *ucontext = (ucontext_t *) ucontext_voidp;
    asm("ebreak");
}

void entrypoint_1(void *sigstack_start) {
    stack_t new_sigstack;
    new_sigstack.ss_flags = 0;
    new_sigstack.ss_sp = sigstack_start;
    new_sigstack.ss_size = SIGSTACK_SIZE;
    s_sigaltstack(&new_sigstack, NULL);

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler;
    for (char *ptr = (char*) &sa.sa_mask; ptr < (char*)(&sa.sa_mask) + sizeof(sigset_t); ptr ++) {
        *ptr = 0;
    }
    s_rt_sigaction(SIGILL, &sa, NULL);

    // FIXME: Trick compiler escape analysis to ensure that the structs are actually written to
    asm("" : : "m"(new_sigstack), "m"(sa) : );

    s_exit_group(0);
}

__attribute__((section(".text.entrypoint")))
void entrypoint() {
    // Get urvirt_config
    struct urvirt_config *conf = (struct urvirt_config *) s_mmap(
        NULL, CONF_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        CONFIG_FD, 0
    );

    s_munmap((void *) 0, (size_t) conf->stub_start);

    conf = (struct urvirt_config *) s_mmap(
        NULL, CONF_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        CONFIG_FD, 0
    );

    if (conf->max_va > (size_t) (conf->stub_start + conf->stub_size)) {
        s_munmap((void *) (conf->stub_start + conf->stub_size), conf->max_va - ((size_t) conf->stub_start + conf->stub_size));
    }

    s_munmap((void *) conf, CONF_SIZE);

    // Set up sigaltstack
    void *sigstack_start = s_mmap(
        NULL, SIGSTACK_SIZE,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
        -1, 0
    );

    // We have another stack now, jump to another function to use it

    asm(
        "mv sp, %[new_sp]\n\t"
        "mv a0, %[stack_start]\n\t"
        "tail entrypoint_1\n\t"
        :
        : [new_sp] "r"(sigstack_start + SIGSTACK_SIZE),
          [stack_start] "r"(sigstack_start)
        :
    );

    // Already jumped away
    __builtin_unreachable();
}
