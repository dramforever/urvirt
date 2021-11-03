#pragma once

#include <stddef.h>
#include <stdint.h>

static const unsigned PRIV_U = 0x00;
static const unsigned PRIV_S = 0x10;
static const unsigned PRIV_M = 0x11;

struct priv_state {
    unsigned priv_mode;

    uintptr_t sstatus;
    uintptr_t sscratch;
    uintptr_t stvec;
    uintptr_t sepc;
    uintptr_t scause;
    uintptr_t stval;
    uintptr_t sip;
    uintptr_t satp;
};

void initialize_priv(struct priv_state *priv);
void handle_priv_instr(struct priv_state *priv, ucontext_t *ucontext, uint32_t instr);

#define ins_bitfield(name, start, width) \
    inline uint32_t ins_##name(uint32_t instr) { \
        return (instr >> start) & ((1 << width) - 1); \
    }

ins_bitfield(opcode, 0, 7)
ins_bitfield(rd, 7, 5)
ins_bitfield(funct3, 12, 3)
ins_bitfield(rs1, 15, 5)
ins_bitfield(rs2, 20, 5)
ins_bitfield(funct7, 25, 7)
ins_bitfield(csr, 20, 12)

#undef ins_bitfield

static const uint32_t OPCODE_SYSTEM = 0b1110011;

static const uint32_t FUNCT7_WFI = 0b001000;
static const uint32_t RS2_WFI = 0b00101;

static const uint32_t FUNCT7_SRET = 0b0001000;
static const uint32_t RS2_SRET = 0b00010;

static const uint32_t FUNCT3_NOT_CSR    = 0b000;

static const uint32_t CSR_USE_UIMM   = 0b100;

static const uint32_t CSR_OP_MASK   = 0b011;
static const uint32_t CSR_OP_RW     = 0b001;
static const uint32_t CSR_OP_RS     = 0b010;
static const uint32_t CSR_OP_RC     = 0b011;

static const uint32_t CSR_SSTATUS   = 0x105;
static const uint32_t CSR_STVEC     = 0x105;
static const uint32_t CSR_SSCRATCH  = 0x140;
static const uint32_t CSR_SEPC      = 0x141;
static const uint32_t CSR_SCAUSE    = 0x142;
static const uint32_t CSR_STVAL     = 0x143;
static const uint32_t CSR_SIP       = 0x144;
static const uint32_t CSR_SATP      = 0x180;
