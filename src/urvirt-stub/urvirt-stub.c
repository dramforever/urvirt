#include <signal.h>
#include <time.h>
#include <ucontext.h>
#include <syscall.h>
#include <fcntl.h>

#include "common.h"
#include "urvirt-syscalls.h"
#include "seccomp-bpf.h"
#include "handle-sbi.h"
#include "riscv-priv.h"
#include "riscv-bits.h"
#include "printf.h"

void _putchar(char character) {
    s_write(2, &character, 1);
}

void handler(int sig, siginfo_t *info, void *ucontext_voidp) {
    ucontext_t *ucontext = (ucontext_t *) ucontext_voidp;
    struct priv_state *priv = (struct priv_state *) s_mmap(
        NULL, CONF_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        CONFIG_FD, 0
    );

    if (sig == SIGSYS) {
        // ecall instruction
        size_t which = info->si_syscall;
        uintptr_t *regs = ucontext->uc_mcontext.__gregs;
        // Fix a7 register
        regs[17] = which;

        if (regs[10] == 0xdeadbeefdeadbeefUL) {
            // magic ecall
            printf("[urvirt] counters\n");
            printf("  ill = %zd\n", priv->counter_ill);
            printf("  segv = %zd\n", priv->counter_segv);
            printf("  sret = %zd\n", priv->counter_sret);
            printf("  uecall = %zd\n", priv->counter_uecall);
            printf("  secall = %zd\n", priv->counter_secall);
        }

        if (priv->priv_mode == PRIV_S) {
            priv->counter_secall ++;

            // SBI call, only legacy ones for now
            uintptr_t ret = handle_legacy_sbi_call(
                priv, which, regs[10], regs[11], regs[12]
            );

            regs[10] = ret;
        } else {
            priv->counter_uecall ++;

            // It's ecall in U-mode, trap to S-mode
            // Fix pc
            // SIGSYS: pc *after* ecall insn
            // RISC-V: pc *at* ecall insn
            regs[0] -= 4;
            enter_trap(priv, ucontext, SCAUSE_UECALL, 0);
        }

    } else if (sig == SIGILL) {
        priv->counter_ill ++;

        if (priv->priv_mode == PRIV_S) {
            // Illegal instruction in S-mode
            // Maybe emulate privileged instruction?

            char *pc = (char *) ucontext->uc_mcontext.__gregs[0];
            // Check first byte of instruction
            // 32-bit insns: last 2 bits are 11, but last 5 bits are not 11111
            if (((*pc) & 0b11) == 0b11 && ((*pc) & 0b11111) != 0b11111) {
                // 32-bit instruction
                uint32_t instr = *(uint32_t *) pc;
                handle_priv_instr(priv, ucontext, instr);
            } else {
                printf("Can't handle in SIGILL at %p\n", pc);
                asm("ebreak");
            }
        } else {
            enter_trap(priv, ucontext, SCAUSE_ILLEGAL, 0);
        }
    } else if (sig == SIGALRM && info->si_code == SI_TIMER) {
        priv->sip = set_six_sti(priv->sip, 1);
        if (priv->priv_mode == PRIV_S) {
            write_log("timer in s mode");
        } else {
            write_log("timer in u mode");
        }

    } else if (sig == SIGSEGV) {
        priv->counter_segv ++;

        uintptr_t scause;
        char *pc = (char *) ucontext->uc_mcontext.__gregs[0];

        if (info->si_addr == pc) {
            // TODO: Handle case of load/store current instruction
            scause = SCAUSE_INSTR_PF;
        } else {
            if (((*pc) & 0b11) != 0b11) {
                uint16_t instr = *((uint16_t *) pc);

                // FIXME: Refactor this instruction detection logic
                if (rvc_looks_like_load(instr)) {
                    scause = SCAUSE_LOAD_PF;
                } else if (rvc_looks_like_store(instr)) {
                    scause = SCAUSE_STORE_PF;
                } else {
                    scause = SCAUSE_INSTR_PF;
                    write_log("weird 16-bit instruction page fault, assuming instr page fault");
                    // asm("ebreak");
                }

            } else {
                uint32_t instr = *((uint32_t *) pc);

                char opcode = (*pc) & 0b1111111;
                if (opcode == OPCODE_LOAD) {
                    scause = SCAUSE_LOAD_PF;
                } else if (opcode == OPCODE_STORE) {
                    scause = SCAUSE_STORE_PF;
                } else if (opcode == OPCODE_AMO) {
                    scause = SCAUSE_STORE_PF;
                } else {
                    scause = SCAUSE_INSTR_PF;
                    write_log("weird 32-bit instruction page fault, assuming instr page fault");
                }
            }
        }

        uintptr_t addr = (uintptr_t)(info->si_addr);
        handle_page_fault(priv, ucontext, scause, addr);
    } else {
        write_log("Don't know how to handle");
        asm("ebreak");
    }

    // Handle interrupt traps

    if (get_six_sti(priv->sip)
        && get_six_sti(priv->sie)
        && (get_sstatus_sie(priv->sstatus) || priv->priv_mode < PRIV_S)) {
        write_log("timer interrupt taken");
        enter_trap(priv, ucontext, SCAUSE_TIMER, 0);
    }

    if (priv->should_clear_vm) {
        priv->should_clear_vm = 0;
        size_t safe_begin = (size_t) priv->stub_start;
        size_t safe_end = (size_t) priv->stub_start + priv->stub_size;
        s_munmap((void *) 0, safe_begin);
        s_munmap((void *) safe_end, (1ull << 38) - safe_end);
    }

    s_munmap(priv, CONF_SIZE);
}

__attribute__((naked)) void handler_wrapper(int sig, siginfo_t *info, void *ucontext_voidp) {
    // This bit of rt_sigreturn(2) code is normally in the vDSO [1] as
    // `__vdso_rt_sigreturn`. Unfortunately we unmapped it earlier so now we
    // need to provide it ourselves.
    //
    // [1]: https://elixir.bootlin.com/linux/latest/source/arch/riscv/kernel/vdso/rt_sigreturn.S

    asm (
        // `ra` would point to `__vdso_rt_sigreturn` at this point, but it's unusable
        "jal %1\n\t"

        // So we make do with our own fake bit of trampoline that is a copy of `__vdso_rt_sigreturn`
        "li a7, %0\n\t"
        "ecall\n\t"
        :
        : "i"(SYS_rt_sigreturn), "S"(handler)
        :
    );
}

void entrypoint_1(void *sigstack_start, struct urvirt_config *conf) {
    // Set stdin to non-blocking
    s_fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    // Set up signal handlers, including new stack and sigmasks

    stack_t new_sigstack;
    new_sigstack.ss_flags = 0;
    new_sigstack.ss_sp = sigstack_start;
    new_sigstack.ss_size = SIGSTACK_SIZE;
    s_sigaltstack(&new_sigstack, NULL);

    struct kernel_sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sa._sa_handler._sa_sigaction = handler_wrapper;
    for (char *ptr = (char*) &sa.sa_mask; ptr < (char*)(&sa.sa_mask) + sizeof(sigset_t); ptr ++) {
        *ptr = 0;
    }

    sa.sa_mask.__bits[0] |= (1 << (SIGALRM - 1));

    s_rt_sigaction(SIGILL, &sa, NULL);
    s_rt_sigaction(SIGSYS, &sa, NULL);
    s_rt_sigaction(SIGALRM, &sa, NULL);
    s_rt_sigaction(SIGSEGV, &sa, NULL);

    // Set up timer to cause SIGALRM when elapses

    struct sigevent sev;
    timer_t timerid;

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_int = 0;

    s_timer_create(CLOCK_REALTIME, &sev, &timerid);

    // At startup, no address translation is done, so map RAM to bare address

    s_mmap(
        (void *) RAM_START, RAM_SIZE,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_SHARED | MAP_FIXED,
        RAM_FD, 0
    );

    // Copy the kernel to RAM

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

    // Set up and enable Seccomp filters

    struct sock_fprog prog;
    struct sock_filter filt[32];
    prog.filter = filt;
    prog.len = gen_addr_filter((size_t) (conf->stub_start), (size_t) (conf->stub_start + conf->stub_size), filt);

    int ret = s_seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog);

    if (ret < 0) {
        s_exit_group(1);
    }

    s_munmap(kernel, kernel_size_pg);

    // Initialize priv_state

    struct priv_state *priv = (struct priv_state *) conf;
    initialize_priv(priv);
    priv->timerid = timerid;

    // Unmap everything that's not the kernel and not the stub
    s_munmap(conf, CONF_SIZE);

    // Here we go
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
    // Juggle around memory maps so that we end up unmapping everything that's
    // not the stub, i.e. ourselves.

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

    void *kernel_end = conf->stub_start + conf->stub_size;

    s_munmap((void *) (conf->stub_start + conf->stub_size), (1ull << 38) - ((size_t) conf->stub_start + conf->stub_size));
    s_munmap((void *) conf, CONF_SIZE);

    // Set up the stack to use, as well as the stack the later part of the initialization needs
    void *sigstack_start = s_mmap(
        kernel_end, SIGSTACK_SIZE,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_FIXED,
        -1, 0
    );


    struct urvirt_config *conf_to_entrypoint_1 = (struct urvirt_config *) s_mmap(
        NULL, CONF_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        CONFIG_FD, 0
    );

    conf_to_entrypoint_1->stub_size += SIGSTACK_SIZE;

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
