#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct priv_state {
    void *stub_start;
    size_t stub_size;
    size_t kernel_size;

    uintptr_t priv_mode;

    uintptr_t sstatus;
    uintptr_t sscratch;
    uintptr_t stvec;
    uintptr_t sepc;
    uintptr_t scause;
    uintptr_t stval;
    uintptr_t sip;
    uintptr_t sie;
    uintptr_t satp;

    _Bool should_clear_vm;
};

void initialize_priv(struct priv_state *priv);
void handle_priv_instr(struct priv_state *priv, ucontext_t *ucontext, uint32_t instr);
void enter_trap(struct priv_state *priv, ucontext_t *ucontext, uintptr_t scause, uintptr_t stval);
