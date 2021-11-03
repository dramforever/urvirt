#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

#include "riscv-priv.h"
#include "urvirt-syscalls.h"

void initialize_priv(struct priv_state *priv) {
    priv->priv_mode = PRIV_S;
    priv->sscratch = 0;
    priv->stvec = 0;
    priv->sepc = 0;
    priv->scause = 0;
    priv->scause = 0;
    priv->sip = 0;
    priv->satp = 0;
    priv->sstatus = 0;
}

uintptr_t read_csr(struct priv_state *priv, uint32_t csr) {
    if (csr == CSR_SSCRATCH) {
        return priv->sscratch;
    } else {
        write_log("Unimplemented CSR");
        asm("ebreak");
    }
}

void write_csr(struct priv_state *priv, uint32_t csr, uintptr_t value) {
    if (csr == CSR_SSCRATCH) {
        priv->sscratch = value;
    } else {
        write_log("Unimplemented CSR");
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

            } else if (ins_funct7(instr) == FUNCT7_SRET && ins_rs2(instr) == RS2_SRET
                && ins_rs1(instr) == 0 && ins_rd(instr) == 0) {

                write_log("Unimplemented sret");
                asm("ebreak");

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
        }
    } else {
        write_log("Invalid instruction: not SYSTEM");
        asm("ebreak");
    }
}
