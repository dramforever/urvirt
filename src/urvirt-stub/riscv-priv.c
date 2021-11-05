#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

#include "riscv-priv.h"
#include "riscv-bits.h"

#include "urvirt-syscalls.h"

static inline uintptr_t sanitize_stvec(uintptr_t stvec) {
    uintptr_t mode = stvec & STVEC_MODE_MASK;
    if (mode == STVEC_DIRECT || mode == STVEC_VECTORED) {
        return stvec;
    } else {
        // Not supported values, WARL
        return stvec & ~ STVEC_MODE_MASK;
    }
}

void initialize_priv(struct priv_state *priv) {
    priv->priv_mode = PRIV_S;
    priv->sscratch = 0;
    priv->stvec = 0;
    priv->sepc = 0;
    priv->scause = 0;
    priv->sip = 0;
    priv->satp = 0;


    priv->sstatus = 0;
    priv->sstatus = set_sstatus_fs(priv->sstatus, SSTATUS_XS_DIRTY);
    priv->sstatus = set_sstatus_xs(priv->sstatus, SSTATUS_XS_DIRTY);
    priv->sstatus = set_sstatus_sd(priv->sstatus, 1);
    priv->sstatus = set_sstatus_sie(priv->sstatus, 1);
}

uintptr_t read_csr(struct priv_state *priv, uint32_t csr) {
    if (csr == CSR_SSCRATCH) {
        return priv->sscratch;
    } else if (csr == CSR_STVEC) {
        return priv->stvec;
    } else if (csr == CSR_SEPC) {
        return priv->sepc;
    } else if (csr == CSR_SSTATUS) {
        return priv->sstatus;
    } else if (csr == CSR_SIE) {
        return priv->sie;
    } else if (csr == CSR_SCAUSE) {
        return priv->scause;
    } else if (csr == CSR_STVAL) {
        return priv->stval;
    } else {
        write_log("Unimplemented CSR read");
        asm("ebreak");
    }
}

void write_csr(struct priv_state *priv, uint32_t csr, uintptr_t value) {
    if (csr == CSR_SSCRATCH) {
        priv->sscratch = value;
    } else if (csr == CSR_STVEC) {
        priv->stvec = sanitize_stvec(value);
    } else if (csr == CSR_SEPC) {
        priv->sepc = value;
    } else if (csr == CSR_SSTATUS) {
        priv->sstatus =
            (value & SSTATUS_WRITABLE_MASK)
            | (priv->sstatus & ~SSTATUS_WRITABLE_MASK);
    } else if (csr == CSR_SIE) {
        write_log("stub: csr sie");
        priv->sie = 0;
    } else if (csr == CSR_SCAUSE) {
        priv->scause = value;
    } else if (csr == CSR_STVAL) {
        priv->stval = value;
    } else {
        write_log("Unimplemented CSR write");
        asm("ebreak");
    }
}

void handle_priv_instr(struct priv_state *priv, ucontext_t *ucontext, uint32_t instr) {
    if (ins_opcode(instr) == OPCODE_SYSTEM) {
        if (ins_funct3(instr) == FUNCT3_NOT_CSR) {
            // Not CSR
            if (ins_funct7(instr) == FUNCT7_SFENCE_VMA && ins_rd(instr) == 0) {

                write_log("Unimplemented sfence.vma");
                asm("ebreak");


            } else if (ins_funct7(instr) == FUNCT7_WFI && ins_rs2(instr) == RS2_WFI
                && ins_rs1(instr) == 0 && ins_rd(instr) == 0) {

                // wfi, do nothing
                ucontext->uc_mcontext.__gregs[0] += 4;

            } else if (ins_funct7(instr) == FUNCT7_SRET && ins_rs2(instr) == RS2_SRET
                && ins_rs1(instr) == 0 && ins_rd(instr) == 0) {
                priv->sstatus = set_sstatus_sie(priv->sstatus, get_sstatus_spie(priv->sstatus));
                priv->sstatus = set_sstatus_spie(priv->sstatus, 1);

                uintptr_t spp = get_sstatus_spp(priv->sstatus);

                priv->sstatus = set_sstatus_spp(priv->sstatus, 0);

                if (spp == SSTATUS_SPP_U) {
                    priv->priv_mode = PRIV_U;
                    priv->should_clear_vm = 1;
                } else {
                    priv->priv_mode = PRIV_S;
                }

                ucontext->uc_mcontext.__gregs[0] = priv->sepc;
            } else {
                write_log("Invalid instruction: non-CSR");
                asm("ebreak");
            }
        } else {
            // CSR instruction

            uint32_t csr = ins_csr(instr);
            uint32_t op = ins_funct3(instr) & CSR_OP_MASK;
            _Bool use_uimm = ins_funct3(instr) & CSR_USE_UIMM;

            uintptr_t *regs = ucontext->uc_mcontext.__gregs;

            uintptr_t write_value =
                use_uimm
                ? ins_rs1(instr)
                : (ins_rs1(instr) == 0 ? 0 : regs[ins_rs1(instr)]);

            _Bool will_write =
                use_uimm ? (write_value != 0) : (ins_rs1(instr) != 0);


            if (op == CSR_OP_RW) {
                if (ins_rd(instr) != 0) {
                    regs[ins_rd(instr)] = read_csr(priv, csr);
                }
                write_csr(priv, csr, write_value);
            } else if (op == CSR_OP_RS) {
                uintptr_t orig = read_csr(priv, csr);
                if (will_write) {
                    write_csr(priv, csr, orig | write_value);
                }
                if (ins_rd(instr) != 0) {
                    regs[ins_rd(instr)] = orig;
                }
            } else if (op == CSR_OP_RC) {
                uintptr_t orig = read_csr(priv, csr);
                if (will_write) {
                    write_csr(priv, csr, orig & ~write_value);
                }
                if (ins_rd(instr) != 0) {
                    regs[ins_rd(instr)] = orig;
                }
            } else {
                write_log("Invalid csr operation");
                asm("ebreak");
            }

            ucontext->uc_mcontext.__gregs[0] += 4;
        }
    } else {
        write_log("Invalid instruction: not SYSTEM");
        asm("ebreak");
    }
}

static inline uintptr_t trap_target(struct priv_state *priv, uintptr_t scause) {
    uintptr_t base = priv->stvec & ~ STVEC_MODE_MASK;
    uintptr_t mode = priv->stvec & STVEC_MODE_MASK;
    if (mode == STVEC_DIRECT || ! (scause & SCAUSE_IS_INT)) {
        return base;
    } else {
        // mode == STVEC_VECTORED
        return base + scause * 4;
    }
}

void enter_trap(struct priv_state *priv, ucontext_t *ucontext, uintptr_t scause, uintptr_t stval) {
    uintptr_t *regs = ucontext->uc_mcontext.__gregs;
    priv->sepc = regs[0];
    priv->scause = scause;
    priv->stval = stval;

    if (priv->priv_mode == PRIV_S) {
        priv->sstatus = set_sstatus_spp(priv->sstatus, SSTATUS_SPP_S);
    } else {
        priv->sstatus = set_sstatus_spp(priv->sstatus, SSTATUS_SPP_U);
    }

    priv->priv_mode = PRIV_S;

    priv->sstatus = set_sstatus_spie(priv->sstatus, get_sstatus_sie(priv->sstatus));
    priv->sstatus = set_sstatus_sie(priv->sstatus, 0);

    regs[0] = trap_target(priv, scause);

    priv->should_clear_vm = 1;
}
