#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Some of the instruction fields

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
static const uint32_t OPCODE_LOAD   = 0b0000011;
static const uint32_t OPCODE_STORE  = 0b0100011;
static const uint32_t OPCODE_AMO    = 0b0101111;

static const uint32_t FUNCT7_WFI = 0b001000;
static const uint32_t RS2_WFI = 0b00101;

static const uint32_t FUNCT7_SRET = 0b0001000;
static const uint32_t RS2_SRET = 0b00010;

static const uint32_t FUNCT7_SFENCE_VMA = 0b0001001;

static const uint32_t FUNCT3_NOT_CSR    = 0b000;

static const uint32_t CSR_USE_UIMM   = 0b100;

static const uint32_t CSR_OP_MASK   = 0b011;
static const uint32_t CSR_OP_RW     = 0b001;
static const uint32_t CSR_OP_RS     = 0b010;
static const uint32_t CSR_OP_RC     = 0b011;

// CSR numbers

static const uint32_t CSR_SSTATUS   = 0x100;
static const uint32_t CSR_SIE       = 0x104;
static const uint32_t CSR_STVEC     = 0x105;
static const uint32_t CSR_SSCRATCH  = 0x140;
static const uint32_t CSR_SEPC      = 0x141;
static const uint32_t CSR_SCAUSE    = 0x142;
static const uint32_t CSR_STVAL     = 0x143;
static const uint32_t CSR_SIP       = 0x144;
static const uint32_t CSR_SATP      = 0x180;

// Privilege modes

static const uintptr_t PRIV_U = 0x00;
static const uintptr_t PRIV_S = 0x10;
static const uintptr_t PRIV_M = 0x11;

// stvec related

static const uintptr_t STVEC_MODE_MASK  = 0b11;
static const uintptr_t STVEC_DIRECT     = 0b00;
static const uintptr_t STVEC_VECTORED   = 0b01;

// scause related

static const uintptr_t SCAUSE_IS_INT    = ((uintptr_t) 1) << 63;

static const uintptr_t SCAUSE_ILLEGAL   = 2;
static const uintptr_t SCAUSE_UECALL    = 8;
static const uintptr_t SCAUSE_INSTR_PF  = 12;
static const uintptr_t SCAUSE_LOAD_PF   = 13;
static const uintptr_t SCAUSE_STORE_PF  = 15;

static const uintptr_t SCAUSE_TIMER     = SCAUSE_IS_INT | 5;

#define bitfield(name, start, width) \
    static const uintptr_t MASK_##name = (( ((uintptr_t) 1) << width ) - 1) << start;\
    static inline uintptr_t get_##name(uintptr_t data) { \
        return (data >> start) & (( ((uintptr_t) 1) << width ) - 1); \
    } \
    static inline uintptr_t set_##name(uintptr_t data, uintptr_t field) { \
        return (data & ~ MASK_##name) | (field << start); \
    }


// sstatus related

bitfield(sstatus_sd, 63, 1);
bitfield(sstatus_uxl, 32, 1);
bitfield(sstatus_mxr, 19, 1);
bitfield(sstatus_sum, 18, 1);
bitfield(sstatus_xs, 15, 2);
bitfield(sstatus_fs, 13, 2);
bitfield(sstatus_spp, 8, 1);
bitfield(sstatus_spie, 5, 1);
bitfield(sstatus_upie, 4, 1);
bitfield(sstatus_sie, 1, 1);
bitfield(sstatus_uie, 0, 1);

static const uintptr_t SSTATUS_XS_OFF       = 0;
static const uintptr_t SSTATUS_XS_INITIAL   = 1;
static const uintptr_t SSTATUS_XS_CLEAN     = 2;
static const uintptr_t SSTATUS_XS_DIRTY     = 3;

static const uintptr_t SSTATUS_SPP_S        = 1;
static const uintptr_t SSTATUS_SPP_U        = 0;

static const uintptr_t SSTATUS_WRITABLE_MASK =
    MASK_sstatus_mxr |
    MASK_sstatus_sum |
    MASK_sstatus_spp |
    MASK_sstatus_spie |
    MASK_sstatus_sie;

// sie and sip related

bitfield(six_sti, 5, 1);

static const uintptr_t SIX_WRITABLE_MASK = MASK_six_sti;

// satp related

bitfield(satp_ppn, 0, 44)
bitfield(satp_asid, 44, 16)
bitfield(satp_mode, 60, 4)

static const uintptr_t SATP_MODE_BARE = 0;
static const uintptr_t SATP_MODE_SV39 = 8;

// PTE fields

bitfield(pte_flags, 0, 8);
bitfield(pte_rsw, 8, 2);
bitfield(pte_ppn, 10, 44);

static const uintptr_t PTE_V = (1 << 0);
static const uintptr_t PTE_R = (1 << 1);
static const uintptr_t PTE_W = (1 << 2);
static const uintptr_t PTE_X = (1 << 3);
static const uintptr_t PTE_U = (1 << 4);
static const uintptr_t PTE_G = (1 << 5);
static const uintptr_t PTE_A = (1 << 6);
static const uintptr_t PTE_D = (1 << 7);

// Virtual address

bitfield(va_ppn0, 30, 9)
bitfield(va_ppn1, 21, 9)
bitfield(va_ppn2, 12, 9)
bitfield(va_off, 0, 12)

#undef bitfield

inline bool rvc_looks_like_load(uint16_t instr) {
    uint16_t op = instr & 0b11;
    uint16_t funct3 = instr >> 13;
    return (op == 0b00 || op == 0b10) && (funct3 == 0b001 || funct3 == 0b010 || funct3 == 0b011);
}

inline bool rvc_looks_like_store(uint16_t instr) {
    uint16_t op = instr & 0b11;
    uint16_t funct3 = instr >> 13;
    return (op == 0b00 || op == 0b10) && (funct3 == 0b101 || funct3 == 0b110 || funct3 == 0b111);
}
