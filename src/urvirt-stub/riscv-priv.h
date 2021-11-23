#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <ucontext.h>

struct priv_state {
    // The start of urvirt_config
    void *stub_start;
    size_t stub_size;
    size_t kernel_size;

    timer_t timerid;        // Start address of the stub

    uintptr_t priv_mode;    // Current privilege mode

    // All the CSRs

    uintptr_t sstatus;
    uintptr_t sscratch;
    uintptr_t stvec;
    uintptr_t sepc;
    uintptr_t scause;
    uintptr_t stval;
    uintptr_t sip;
    uintptr_t sie;
    uintptr_t satp;

    // Should we reset virtual memory mappings before returning from signal
    // handler and executing the next instruction?
    bool should_clear_vm;
};

void initialize_priv(struct priv_state *priv);
void handle_priv_instr(struct priv_state *priv, ucontext_t *ucontext, uint32_t instr);
void enter_trap(struct priv_state *priv, ucontext_t *ucontext, uintptr_t scause, uintptr_t stval);

// Handle SIGSEGV. If permissions allow, map memory and retry, otherwise
// generate page fault trap
//
// TODO: Emulation of load/store instructions
//
// \param stval The faulting address
void handle_page_fault(struct priv_state *priv, ucontext_t *ucontext, uintptr_t scause, uintptr_t stval);

