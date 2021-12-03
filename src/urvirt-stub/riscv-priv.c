#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

#include "riscv-priv.h"
#include "riscv-bits.h"
#include "common.h"
#include "printf.h"
#include "urvirt-block.h"

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
    priv->satp = 0;
    priv->sip = 0;
    priv->sie = 0;

    priv->sstatus = 0;
    priv->sstatus = set_sstatus_fs(priv->sstatus, SSTATUS_XS_DIRTY);
    priv->sstatus = set_sstatus_xs(priv->sstatus, SSTATUS_XS_DIRTY);
    priv->sstatus = set_sstatus_sd(priv->sstatus, 1);
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
    } else if (csr == CSR_SIP) {
        return priv->sip;
    } else if (csr == CSR_SCAUSE) {
        return priv->scause;
    } else if (csr == CSR_STVAL) {
        return priv->stval;
    } else if (csr == CSR_SATP) {
        return priv->satp;
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
        priv->sie =
            (value & SIX_WRITABLE_MASK)
            | (priv->sie & ~ SIX_WRITABLE_MASK);
    } else if (csr == CSR_SIP) {
        priv->sip =
            (value & SIX_WRITABLE_MASK)
            | (priv->sip & ~ SIX_WRITABLE_MASK);
    } else if (csr == CSR_SCAUSE) {
        priv->scause = value;
    } else if (csr == CSR_STVAL) {
        priv->stval = value;
    } else if (csr == CSR_SATP) {
        if (get_satp_mode(value) == SATP_MODE_SV39 || get_satp_mode(value) == SATP_MODE_BARE) {
            priv->satp = set_satp_asid(value, 0);
            priv->should_clear_vm = 1;
        }
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

                // sfence.vma
                priv->should_clear_vm = 1;
                ucontext->uc_mcontext.__gregs[0] += 4;

            } else if (ins_funct7(instr) == FUNCT7_WFI && ins_rs2(instr) == RS2_WFI
                && ins_rs1(instr) == 0 && ins_rd(instr) == 0) {

                // wfi, do nothing
                ucontext->uc_mcontext.__gregs[0] += 4;

            } else if (ins_funct7(instr) == FUNCT7_SRET && ins_rs2(instr) == RS2_SRET
                && ins_rs1(instr) == 0 && ins_rd(instr) == 0) {

                // sret
                uintptr_t spp = get_sstatus_spp(priv->sstatus);

                priv->sstatus = set_sstatus_sie(priv->sstatus, get_sstatus_spie(priv->sstatus));
                priv->sstatus = set_sstatus_spie(priv->sstatus, 1);

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

// This is the page table walker. It's ugly. Sorry.
static bool lookup_pa_in_ram(struct priv_state *priv, void *ram, uintptr_t va, uintptr_t *pa, uint64_t *pte) {
    if (get_satp_mode(priv->satp) == SATP_MODE_BARE) {
        *pa = va;
        *pte = 0;
        return true;
    }

    intptr_t va_signed = va;
    // Check virtual address sign extension
    if (va_signed != (va_signed << (63 - 39) >> (63 - 39)))
        return false;

    uintptr_t off = get_va_off(va);
    uintptr_t vpn[3] = { get_va_ppn0(va), get_va_ppn1(va), get_va_ppn2(va) };

    uintptr_t pt_addr = get_satp_ppn(priv->satp) << 12;

    for (int level = 0; level < 3; level ++) {
        if (pt_addr < RAM_START || pt_addr >= RAM_START + RAM_SIZE) {
            write_log("pt address out of range");
            asm("ebreak");
            return false;
        }

        uint64_t entry = *(uint64_t *)(ram + (pt_addr - RAM_START) + (vpn[level] * 8));

#define fl(f) (get_pte_flags(entry) & PTE_##f)

        if (! fl(V)) return false;
        if ((! fl(R)) && fl(W)) return false;

        if (level < 2) {
            // Should be pointer to next level page table
            if (fl(R) || fl(W) || fl(X)) {
                write_log("Unimplemented large pages");
                asm("ebreak");
                return false;
            }

            pt_addr = get_pte_ppn(entry) << 12;
        } else {
            if (fl(W) && ! fl(R)) {
                return false;
            }
            *pa = (get_pte_ppn(entry) << 12) | get_va_off(va);
            *pte = entry;
            return true;
        }

#undef fl

    }
}

bool lookup_pa(struct priv_state *priv, uintptr_t va, uintptr_t *pa, uint64_t *pte) {
    void *ram = s_mmap(
        NULL, RAM_SIZE,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_SHARED,
        RAM_FD, 0
    );

    bool res = lookup_pa_in_ram(priv, ram, va, pa, pte);

    s_munmap(ram, RAM_SIZE);

    return res;
}

static inline int decode_sd(char *pc) {
    if ((*pc) & 0b11 == 0b11) {
        uint32_t instr = *(uint32_t *) pc;

        if (ins_opcode(instr) == OPCODE_STORE) {
            if (ins_funct3(instr) == 0b011) {
                return ins_rs2(instr);
            } else {
                return -1;
            }
        }
    } else {
        uint16_t instr = *(uint16_t *) pc;

        uint16_t op = instr & 0b11;
        uint16_t funct3 = instr >> 13;

        if (op == 0b00 && funct3 == 0b111) {
            // c.sd
            return ((instr >> 2) & 0b111) | 8;
        } else if (op == 0b10 && funct3 == 0b011) {
            // c.sdsp
            return (instr >> 2) & 0b11111;
        } else {
            return -1;
        }
    }
}

static inline bool get_store_data(ucontext_t *ucontext, uintptr_t *data) {
    uintptr_t pc = ucontext->uc_mcontext.__gregs[0];
    int rs2 = decode_sd((char *) pc);
    if (rs2 >= 0) {
        *data = ucontext->uc_mcontext.__gregs[rs2];
        return true;
    } else {
        return false;
    }
}

void handle_page_fault(struct priv_state *priv, ucontext_t *ucontext, uintptr_t scause, uintptr_t stval) {
    uintptr_t pa;
    uint64_t pte;
    bool found = lookup_pa(priv, stval, &pa, &pte);
    if (found) {
        if (pte == 0) {
            // No translation
            write_log("someone turned off translation");
            asm("ebreak");
        } else {
            uintptr_t ppn = get_pte_ppn(pte);
            if (pa >= RAM_START && pa < RAM_START + RAM_SIZE) {
                int flags = 0;

                if ((scause == SCAUSE_INSTR_PF && ! (get_pte_flags(pte) & PTE_X))
                    || (scause == SCAUSE_LOAD_PF && ! (get_pte_flags(pte) & PTE_R))
                    || (scause == SCAUSE_STORE_PF && ! (get_pte_flags(pte) & PTE_W))) {
                    // Permission denied
                    if (priv->priv_mode == PRIV_S) {
                        write_log("page permission denied in s mode");
                        asm("ebreak");
                    } else {
                        write_log("page permission denied in u mode");
                    }
                    enter_trap(priv, ucontext, scause, stval);
                } else {
                    if (get_pte_flags(pte) & PTE_R) flags |= PROT_READ;
                    if (get_pte_flags(pte) & PTE_W) flags |= PROT_WRITE;
                    if (get_pte_flags(pte) & PTE_X) flags |= PROT_EXEC;

                    void *res = s_mmap(
                        (void *) (((uintptr_t) stval) & ~((1 << 12) - 1)), 4096,
                        flags,
                        MAP_SHARED | MAP_FIXED_NOREPLACE,
                        RAM_FD, (ppn << 12) - RAM_START
                    );
                    if ((intptr_t)(res) < 0) {
                        // Linux mmap(2) does not like us mapping like literally half the address in Sv39 space.
                        //
                        // Honestly, not much we can do with that.
                        printf("[urvirt] Trying to mmap failed, addr=0x%zx, errno=%d\n", stval, - (int)(intptr_t)(res));
                        asm("ebreak");
                    }
                }
            } else {
                uintptr_t pc = ucontext->uc_mcontext.__gregs[0];

                uintptr_t store_data;

                if (scause == SCAUSE_STORE_PF) {
                    if (! get_store_data(ucontext, &store_data)) {
                        printf("[urvirt] Failed to get MMIO store data\n");
                        asm("ebreak");
                    }
                }

                if (pa == URVIRT_BLOCK + URVIRT_BLOCK_COMMAND && scause == SCAUSE_STORE_PF) {
                    printf("[urvirt] urvirt block command %zd, block_id=%zd, buf=0x%zx\n",
                        store_data, priv->urvb_block_id, priv->urvb_buf);
                    if (store_data == URVIRT_BLOCK_CMD_READ) {
                        s_pread64(BLOCK_FD, (void *) priv->urvb_buf, URVIRT_BLOCK_SIZE, URVIRT_BLOCK_SIZE * priv->urvb_block_id);
                    } else if (store_data == URVIRT_BLOCK_CMD_WRITE) {
                        s_pwrite64(BLOCK_FD, (const void *) priv->urvb_buf, URVIRT_BLOCK_SIZE, URVIRT_BLOCK_SIZE * priv->urvb_block_id);
                    } else {
                        asm("ebreak");
                    }
                    if ((*(char *) pc) & 0b11 == 0b11)
                        ucontext->uc_mcontext.__gregs[0] += 4;
                    else
                        ucontext->uc_mcontext.__gregs[0] += 2;

                } else if (pa == URVIRT_BLOCK + URVIRT_BLOCK_BLOCK_ID && scause == SCAUSE_STORE_PF) {
                    priv->urvb_block_id = store_data;
                    if ((*(char *) pc) & 0b11 == 0b11)
                        ucontext->uc_mcontext.__gregs[0] += 4;
                    else
                        ucontext->uc_mcontext.__gregs[0] += 2;
                } else if (pa == URVIRT_BLOCK + URVIRT_BLOCK_BUF && scause == SCAUSE_STORE_PF) {
                    priv->urvb_buf = store_data;
                    if ((*(char *) pc) & 0b11 == 0b11)
                        ucontext->uc_mcontext.__gregs[0] += 4;
                    else
                        ucontext->uc_mcontext.__gregs[0] += 2;
                } else {
                    printf("[urvirt] Page fault, sepc=0x%zx, rs2=x%d, va=0x%zx, pa=0x%zx, scause=%d cannot handle\n",
                        (size_t) pc, decode_sd((char *) pc),
                        stval, pa, scause
                    );

                    asm("ebreak");
                }
            }
        }
    } else {

        // Looks like we got a real page fault
        enter_trap(priv, ucontext, scause, stval);
    }
}
