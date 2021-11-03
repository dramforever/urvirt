#include <signal.h>
#include <ucontext.h>
#include <syscall.h>

#include "common.h"
#include "urvirt-syscalls.h"
#include "seccomp-bpf.h"
#include "handle-sbi.h"
#include "riscv-priv.h"

void handler(int sig, siginfo_t *info, void *ucontext_voidp) {
    ucontext_t *ucontext = (ucontext_t *) ucontext_voidp;
    struct priv_state *priv = (struct priv_state *) s_mmap(
        NULL, CONF_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        CONFIG_FD, 0
    );

    if (sig == SIGSYS) {
        size_t which = info->si_syscall;
        uintptr_t *regs = ucontext->uc_mcontext.__gregs;
        struct sbiret ret = handle_sbi_call(
            which, regs[10], regs[11], regs[12]
        );
        regs[10] = ret.error;
        regs[11] = ret.value;
    } else if (sig == SIGILL) {
        // Instruction
        char *pc = (char *) ucontext->uc_mcontext.__gregs[0];
        if (((*pc) & 0b11) == 0b11 && ((*pc) & 0b11111) != 0b11111) {
            // 32-bit instruction
            uint32_t instr = *(uint32_t *) pc;
            handle_priv_instr(priv, ucontext, instr);
            // Next instruction
            ucontext->uc_mcontext.__gregs[0] += 4;
        } else {
            write_log("Can't handle in SIGILL");
            asm("ebreak");
        }
    } else {
        write_log("Don't know how to handle");
        asm("ebreak");
    }

    s_munmap(priv, CONF_SIZE);
}

__attribute__((naked)) void handler_wrapper(int sig, siginfo_t *info, void *ucontext_voidp) {
    asm (
        "jal %1\n\t"
        "li a7, %0\n\t"
        "ecall\n\t"
        :
        : "i"(SYS_rt_sigreturn), "S"(handler)
        :
    );
}

void entrypoint_1(void *sigstack_start, struct urvirt_config *conf) {
    stack_t new_sigstack;
    new_sigstack.ss_flags = 0;
    new_sigstack.ss_sp = sigstack_start;
    new_sigstack.ss_size = SIGSTACK_SIZE;
    s_sigaltstack(&new_sigstack, NULL);

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler_wrapper;
    for (char *ptr = (char*) &sa.sa_mask; ptr < (char*)(&sa.sa_mask) + sizeof(sigset_t); ptr ++) {
        *ptr = 0;
    }
    s_rt_sigaction(SIGILL, &sa, NULL);
    s_rt_sigaction(SIGSYS, &sa, NULL);

    s_mmap(
        (void *) RAM_START, RAM_SIZE,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_SHARED | MAP_FIXED,
        RAM_FD, 0
    );

    size_t kernel_size_pg = (conf->kernel_size + 4095) & (~ 4095);

    char *kernel = s_mmap(
        NULL, kernel_size_pg,
        PROT_READ,
        MAP_PRIVATE,
        KERNEL_FD, 0
    );

    for (size_t i = 0; i < conf->kernel_size; i ++) {
        ((char *) KERNEL_START)[i] = kernel[i];
    }

    s_riscv_flush_icache(0, 0, 0);

    struct sock_fprog prog;
    struct sock_filter filt[32];
    prog.filter = filt;
    prog.len = gen_addr_filter((size_t) (conf->stub_start), (size_t) (conf->stub_start + conf->stub_size), filt);

    int ret = s_seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog);

    if (ret < 0) {
        s_exit_group(1);
    }

    s_munmap(kernel, kernel_size_pg);

    struct priv_state *priv = (struct priv_state *) conf;
    initialize_priv(priv);

    s_munmap(conf, CONF_SIZE);

    write_log("Jumping to kernel ...");

    asm volatile (
        "jr %[start]\n\t"
        :
        : [start] "r"(KERNEL_START)
        :
    );

    asm ("" ::: "memory");

    // Already jumped away
    __builtin_unreachable();
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
    s_munmap((void *) conf, CONF_SIZE);

    conf = (struct urvirt_config *) s_mmap(
        NULL, CONF_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        CONFIG_FD, 0
    );

    s_munmap((void *) (conf->stub_start + conf->stub_size), (1ull << 38) - ((size_t) conf->stub_start + conf->stub_size));
    s_munmap((void *) conf, CONF_SIZE);

    // Set up sigaltstack
    void *sigstack_start = s_mmap(
        NULL, SIGSTACK_SIZE,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
        -1, 0
    );


    void *conf_to_entrypoint_1 = (struct urvirt_config *) s_mmap(
        NULL, CONF_SIZE,
        PROT_READ | PROT_WRITE, MAP_PRIVATE,
        CONFIG_FD, 0
    );

    // We have another stack now, jump to another function to use it

    asm volatile (
        "mv sp, %[new_sp]\n\t"
        "mv a0, %[stack_start]\n\t"
        "mv a1, %[conf]\n\t"
        "tail %[entry]\n\t"
        :
        : [new_sp] "r"(sigstack_start + SIGSTACK_SIZE),
          [stack_start] "r"(sigstack_start),
          [conf] "r"(conf_to_entrypoint_1),
          [entry] "S"(entrypoint_1)
        : "a0", "a1"
    );

    // Already jumped away
    __builtin_unreachable();
}
