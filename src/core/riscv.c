#include "core/riscv.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/instr.h"
#include "device/device.h"
#include <plat/plat.h>

void riscv_csr_init (riscv_t *riscv) {
    // the regs here actually unused
    // model version
    riscv->csr_regs.marchid = 0xDC68D886;
    riscv->csr_regs.mimpid = 0xdc688001;
}

riscv_word_t riscv_read_csr (riscv_t * riscv, riscv_word_t csr) {
    switch (csr) {
        case CSR_MARCHID:
            return riscv->csr_regs.marchid;
        case CSR_MPIDID:
            return riscv->csr_regs.mimpid;
        case CSR_MSTATUS:
            return riscv->csr_regs.mstatus;
        case CSR_MTVEC:
            return riscv->csr_regs.mtvec;
        case CSR_MSCRATCH:
            return riscv->csr_regs.mscratch;
        case CSR_MEPC:
            return riscv->csr_regs.mepc;
        case CSR_MCAUSE:
            return riscv->csr_regs.mcause;
        case CSR_MTVAL:
            return riscv->csr_regs.mtval;
        default:
            return 0;            
    }
}

void riscv_write_csr (riscv_t * riscv, riscv_word_t csr, riscv_word_t val) {
    switch (csr) {
        case CSR_MSTATUS:
             riscv->csr_regs.mstatus = val;
             break;
        case CSR_MTVEC:
            riscv->csr_regs.mtvec = val;
            break;
        case CSR_MSCRATCH:
            riscv->csr_regs.mscratch = val;
            break;
        case CSR_MEPC:
            riscv->csr_regs.mepc = val;
            break;
        case CSR_MCAUSE:
            riscv->csr_regs.mcause = val;
            break;
        case CSR_MTVAL:
            riscv->csr_regs.mtval = val;
            break;
        default:
            break;           
    }    
}

// create uninitialized riscv
riscv_t *riscv_create(void) {
    riscv_t *riscv = calloc(1, sizeof(riscv_t));
    if (!riscv) {
        fprintf(stderr, "alloc riscv failed\n");
        return riscv;
    }

    return riscv;
}

void riscv_add_device(riscv_t *riscv, device_t *device) {
    device->next = riscv->device_list;
    riscv->device_list = device;
    device->riscv = riscv;
}

void riscv_set_flash(riscv_t *riscv, mem_t *flash) {
    riscv->flash = flash;
}

void riscv_set_pfic(riscv_t *riscv, pfic_t *pfic) {
    riscv->pfic = pfic;
}

void riscv_load_bin(riscv_t *riscv, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "open bin file failed, path=%s\n", path);
        exit(-1);
    }

    if (feof(file)) {
        fprintf(stderr, "opened bin file is empty, path=%s\n", path);
        fclose(file);
        exit(-1);
    }

    size_t total = 0;
    size_t read_size;
    while (read_size = fread(riscv->flash->mem + total, 1, 1024, file)) {
        total += read_size;
    }
    
    fclose(file);
}

void riscv_load_elf(riscv_t *riscv, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "open file %s failed\n", path);
        exit(-1);
    }

    Elf32_Ehdr elf_hdr;
    size_t hdr_read = fread(&elf_hdr, 1, sizeof(Elf32_Ehdr), file);
    if (hdr_read < sizeof(Elf32_Ehdr)) {
        fprintf(stderr, "read elf file %s header failed\n", path);
        exit(-1);
    }

    for (int i = 0; i < elf_hdr.e_phnum; i++) {
        fseek(file, elf_hdr.e_phoff + sizeof(Elf32_Phdr) * i, SEEK_SET);
        Elf32_Phdr elf_phdr;
        size_t phdr_read = fread(&elf_phdr, 1, sizeof(Elf32_Phdr), file);
        if (phdr_read < sizeof(Elf32_Phdr)) {
            fprintf(stderr, "read elf file %s phdr failed\n", path);
            exit(-1);
        }

        if (elf_phdr.p_type != PT_LOAD) {
            continue;
        }

        int sec_size = elf_phdr.p_filesz;
        char *buf = calloc(1, sec_size);
        fseek(file, elf_phdr.p_offset, SEEK_SET);
        size_t sec_read = fread(buf, 1, sec_size, file);
        if (sec_read < sec_size) {
            fprintf(stderr, "read elf file %s sections failed\n", path);
            exit(-1);
        }
        riscv_mem_write(riscv, elf_phdr.p_paddr, buf, sec_size);
        free(buf);
    }
}

void riscv_reset(riscv_t *riscv) {
    riscv->pc = 0;
    riscv->instr.raw = 0;
    riscv->dev_read = riscv->dev_write = (device_t *)0;
    memset(riscv->regs, 0, sizeof(riscv->regs));
    riscv_csr_init(riscv);
}

static void execute_EBREAK(riscv_t *riscv, instr_t *instr) {
    return;
}

// remains positivity or negativity
static inline int32_t i_get_imm(instr_t *instr) {
    return (instr->i.imm_11_0 & (1 << 11)) ? (instr->i.imm_11_0 | 0xFFFFF << 12) : instr->i.imm_11_0;
}

// imm is put in the first 20 bits rather than the last 20 bits
static inline int32_t u_get_imm(instr_t *instr) {
    return instr->u.imm_31_12 << 12;
}

static inline int32_t s_get_imm(instr_t *instr) {
    int extend_1 = instr->s.imm_11_5 & (1 << 6);
    riscv_word_t imm = (instr->s.imm_11_5 << 5) | instr->s.imm_4_0;
    return extend_1 ? (imm | 0xFFFFF << 12) : imm;
}

static inline int32_t j_get_imm(instr_t *instr) {
    riscv_word_t imm = (instr->j.imm_10_1 << 1) | (instr->j.imm_11 << 11) |
     (instr->j.imm_19_12 << 12) | (instr->j.imm_20 << 20);
    
    return instr->j.imm_20 ? (imm | (0x7FF << 21)) : imm; 
}

static inline int32_t b_get_imm(instr_t *instr) {
    int32_t imm = (instr->b.imm_12 << 12) | (instr->b.imm_11 << 11) |
     (instr->b.imm_10_5 << 5) | (instr->b.imm_4_1 << 1);
    
    return instr->b.imm_12 ? (imm | (0x1FFFFFFF << 13)) : imm;
}

static void execute_ADDI(riscv_t *riscv, instr_t *instr) {
    int32_t imm = i_get_imm(instr);
    riscv_word_t rd = instr->i.rd;
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val + imm); // if rd invalid?
}

static void execute_ORI(riscv_t *riscv, instr_t *instr) {
    int32_t imm = i_get_imm(instr);
    riscv_word_t rd = instr->i.rd;
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val | imm); // if rd invalid?
}

static void execute_ANDI(riscv_t *riscv, instr_t *instr) {
    int32_t imm = i_get_imm(instr);
    riscv_word_t rd = instr->i.rd;
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val & imm); // if rd invalid?
}

static void execute_XORI(riscv_t *riscv, instr_t *instr) {
    int32_t imm = i_get_imm(instr);
    riscv_word_t rd = instr->i.rd;
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val ^ imm); // if rd invalid?
}

// remember only shift right has to consider the polarity 
static void execute_SLLI(riscv_t *riscv, instr_t *instr) {
    riscv_word_t shamt = instr->i.imm_11_0 & 0x1F;
    riscv_word_t rd = instr->i.rd;
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val << shamt); // if rd invalid?
}

// in c, >> do sra on signed integers 
// and do srl on unsigned integers
static void execute_SRLI(riscv_t *riscv, instr_t *instr) {
    riscv_word_t shamt = instr->i.imm_11_0 & 0x1F;
    riscv_word_t rd = instr->i.rd;
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val >> shamt); // if rd invalid?
}

static void execute_SRAI(riscv_t *riscv, instr_t *instr) {
    riscv_word_t shamt = instr->i.imm_11_0 & 0x1F;
    riscv_word_t rd = instr->i.rd;
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val >> shamt); // if rd invalid?
}

static void execute_SLTI(riscv_t *riscv, instr_t *instr) {
    int32_t imm = i_get_imm(instr);
    riscv_word_t rd = instr->i.rd;
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val < imm); // if rd invalid?
}

// R[rd] = (R[rs1] <(u) SignExt(imm12))? 1 : 0
static void execute_SLTIU(riscv_t *riscv, instr_t *instr) {
    riscv_word_t imm = i_get_imm(instr);
    riscv_word_t rd = instr->i.rd;
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->i.rs1); // if rs1 invalid?
    
    riscv_write_reg(riscv, rd, rs1_val < imm); // if rd invalid?
}

static void execute_ADD(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int32_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val + rs2_val); // if rd invalid?
}

static void execute_SUB(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int32_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val - rs2_val); // if rd invalid?
}

// keep the least significant 32 bits
static void execute_MUL(riscv_t *riscv, instr_t *instr) {
    int64_t rs1_val = (int64_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int64_t rs2_val = (int64_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, (riscv_word_t)(rs1_val * rs2_val)); // if rd invalid?
}

// keep the most significant 32 bits
static void execute_MULH(riscv_t *riscv, instr_t *instr) {
    int64_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int64_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, (riscv_word_t)((rs1_val * rs2_val) >> 32)); // if rd invalid?
}

static void execute_MULHSU(riscv_t *riscv, instr_t *instr) {
    int64_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    uint64_t rs2_val = riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, (riscv_word_t)((rs1_val * rs2_val) >> 32)); // if rd invalid?
}

static void execute_MULHU(riscv_t *riscv, instr_t *instr) {
    uint64_t rs1_val = riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    uint64_t rs2_val = riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, (riscv_word_t)((rs1_val * rs2_val) >> 32)); // if rd invalid?
}

static void execute_DIV(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int32_t rs2_val = riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val / rs2_val); // if rd invalid?
}

static void execute_DIVU(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val / rs2_val); // if rd invalid?
}

static void execute_REM(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int32_t rs2_val = riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val % rs2_val); // if rd invalid?
}

static void execute_REMU(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val % rs2_val); // if rd invalid?
}

static void execute_OR(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int32_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val | rs2_val); // if rd invalid?
}

static void execute_AND(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int32_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val & rs2_val); // if rd invalid?
}

static void execute_XOR(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int32_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val ^ rs2_val); // if rd invalid?
}

static void execute_SLL(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val << rs2_val); // if rd invalid?
}

static void execute_SLT(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    int32_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val < rs2_val); // if rd invalid?
}

static void execute_SLTU(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val < rs2_val); // if rd invalid?
}

static void execute_SRL(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    riscv_word_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val >> rs2_val); // if rd invalid?
}

static void execute_SRA(riscv_t *riscv, instr_t *instr) {
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->r.rs1); // if rs1 invalid?
    riscv_word_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->r.rs2); // if rs2 invalid?
    riscv_write_reg(riscv, instr->r.rd, rs1_val >> rs2_val); // if rd invalid?
}

// Load upper immediate; U; lui rd, imm20; R[rd] = SignExt(imm20 << 12)
static void execute_LUI(riscv_t *riscv, instr_t *instr) {
    riscv_word_t imm = u_get_imm(instr);
    riscv_write_reg(riscv, instr->u.rd, imm); // if rd invalid?
}

static void execute_AUIPC(riscv_t *riscv, instr_t *instr) {
    riscv_word_t imm = u_get_imm(instr);
    riscv_write_reg(riscv, instr->u.rd, imm + riscv->pc); // if rd invalid?
}

// notice that when doing add, signed or unsigned doesn't matter
// only matters when comparing
static void execute_SB(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->s.rs1);
    riscv_word_t addr = rs1_val + s_get_imm(instr);
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->s.rs2);
    riscv_mem_write(riscv, addr, (uint8_t*)&rs2_val, 1);
}

static void execute_SH(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->r.rs1);
    riscv_word_t addr = rs1_val + s_get_imm(instr);
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->r.rs2);
    riscv_mem_write(riscv, addr, (uint8_t*)&rs2_val, 2);
}

static void execute_SW(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->r.rs1);
    riscv_word_t addr = rs1_val + s_get_imm(instr);
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->r.rs2);
    riscv_mem_write(riscv, addr, (uint8_t*)&rs2_val, 4);
}

// load byte, load half word must do sign extension after loading
// the imm in load/store can be either positive or negative
static void execute_LB(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->i.rs1);
    riscv_word_t addr = rs1_val + i_get_imm(instr);
    riscv_word_t byte = 0; // important to reset to zero
    riscv_mem_read(riscv, addr, (uint8_t*)&byte, 1);
    byte = byte & (1 << 7) ? (byte | (0xFFFFFF << 8)) : byte;
    riscv_write_reg(riscv, instr->i.rd, byte);
}

static void execute_LBU(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->i.rs1);
    riscv_word_t addr = rs1_val + i_get_imm(instr);
    riscv_word_t byte = 0;
    riscv_mem_read(riscv, addr, (uint8_t*)&byte, 1);
    riscv_write_reg(riscv, instr->i.rd, byte);
}

static void execute_LH(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->i.rs1);
    riscv_word_t addr = rs1_val + i_get_imm(instr);
    riscv_word_t hw;
    riscv_mem_read(riscv, addr, (uint8_t*)&hw, 2);
    hw = hw & (1 << 15) ? (hw | (0xFFFFFF << 16)) : hw;
    riscv_write_reg(riscv, instr->i.rd, hw);
}

static void execute_LHU(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->i.rs1);
    riscv_word_t addr = rs1_val + i_get_imm(instr);
    riscv_word_t hw = 0;
    riscv_mem_read(riscv, addr, (uint8_t*)&hw, 2);
    riscv_write_reg(riscv, instr->i.rd, hw);
}

static void execute_LW(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->i.rs1);
    riscv_word_t addr = rs1_val + i_get_imm(instr);
    riscv_word_t word;
    riscv_mem_read(riscv, addr, (uint8_t*)&word, 4);
    riscv_write_reg(riscv, instr->i.rd, word);
}

static void execute_JAL(riscv_t *riscv, instr_t *instr) {
    int32_t imm = j_get_imm(instr);
    riscv_write_reg(riscv, instr->j.rd, riscv->pc + 4);
    riscv->pc += imm;
}

static void execute_JALR(riscv_t *riscv, instr_t *instr) {
    int32_t imm = i_get_imm(instr);
    int32_t rs1_val = riscv_read_reg(riscv, instr->i.rs1);
    riscv_write_reg(riscv, instr->i.rd, riscv->pc + 4);
    riscv->pc = (rs1_val + imm);
}

static void execute_BEQ(riscv_t *riscv, instr_t *instr) {
    int32_t imm = b_get_imm(instr);
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->b.rs1);
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->b.rs2);
    if (rs1_val == rs2_val) {
        riscv->pc += imm;
        return;
    }
    riscv->pc += 4;
}

static void execute_BGE(riscv_t *riscv, instr_t *instr) {
    int32_t imm = b_get_imm(instr);
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->b.rs1);
    int32_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->b.rs2);
    if (rs1_val >= rs2_val) {
        riscv->pc += imm;
        return;
    }   
    riscv->pc += 4; 
}

static void execute_BGEU(riscv_t *riscv, instr_t *instr) {
    int32_t imm = b_get_imm(instr);
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->b.rs1);
    riscv_word_t rs2_val = riscv_read_reg(riscv, instr->b.rs2);
    if (rs1_val >= rs2_val) {
        riscv->pc += imm;
        return;
    }      
    riscv->pc += 4; 
}

static void execute_BLT(riscv_t *riscv, instr_t *instr) {
    int32_t imm = b_get_imm(instr);
    int32_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->b.rs1);
    int32_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->b.rs2);
    if (rs1_val < rs2_val) {
        riscv->pc += imm;
        return;
    }   
    riscv->pc += 4;
}

static void execute_BLTU(riscv_t *riscv, instr_t *instr) {
    int32_t imm = b_get_imm(instr);
    riscv_word_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->b.rs1);
    riscv_word_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->b.rs2);
    if (rs1_val < rs2_val) {
        riscv->pc += imm;
        return;
    }       
    riscv->pc += 4;  
}

static void execute_BNE(riscv_t *riscv, instr_t *instr) {
    int32_t imm = b_get_imm(instr);
    riscv_word_t rs1_val = (int32_t)riscv_read_reg(riscv, instr->b.rs1);
    riscv_word_t rs2_val = (int32_t)riscv_read_reg(riscv, instr->b.rs2);
    if (rs1_val != rs2_val) {
        riscv->pc += imm;
        return;
    }
    riscv->pc += 4;         
}

static void execute_CSRRW(riscv_t *riscv, instr_t *instr) {
    riscv_word_t csr_addr = i_get_imm(instr);
    riscv_word_t old_csr = riscv_read_csr(riscv, csr_addr);
    riscv_word_t new_csr = riscv_read_reg(riscv, instr->i.rs1);
    riscv_write_reg(riscv, instr->i.rd, old_csr);
    riscv_write_csr(riscv, csr_addr, new_csr); 
}

static void execute_CSRRS(riscv_t *riscv, instr_t *instr) {
    riscv_word_t csr_addr = i_get_imm(instr);
    riscv_word_t old_csr = riscv_read_csr(riscv, csr_addr);
    riscv_word_t new_csr = old_csr | riscv_read_reg(riscv, instr->i.rs1);
    riscv_write_reg(riscv, instr->i.rd, old_csr);
    riscv_write_csr(riscv, csr_addr, new_csr); 
}

static void execute_CSRRC(riscv_t *riscv, instr_t *instr) {
    riscv_word_t csr_addr = i_get_imm(instr);
    riscv_word_t old_csr = riscv_read_csr(riscv, csr_addr);
    riscv_word_t new_csr = old_csr & (~riscv_read_reg(riscv, instr->i.rs1));
    riscv_write_reg(riscv, instr->i.rd, old_csr);
    riscv_write_csr(riscv, csr_addr, new_csr); 
}

static void execute_CSRRWI(riscv_t *riscv, instr_t *instr) {
    riscv_word_t csr_addr = i_get_imm(instr);
    riscv_word_t old_csr = riscv_read_csr(riscv, csr_addr);
    riscv_word_t uimm = instr->i.rs1;
    riscv_write_reg(riscv, instr->i.rd, old_csr);
    riscv_write_csr(riscv, csr_addr, uimm); 
}

static void execute_CSRRSI(riscv_t *riscv, instr_t *instr) {
    riscv_word_t csr_addr = i_get_imm(instr);
    riscv_word_t old_csr = riscv_read_csr(riscv, csr_addr);
    riscv_word_t uimm = instr->i.rs1;
    riscv_word_t new_csr = old_csr | uimm;
    riscv_write_reg(riscv, instr->i.rd, old_csr);
    riscv_write_csr(riscv, csr_addr, new_csr); 
}

static void execute_CSRRCI(riscv_t *riscv, instr_t *instr) {
    riscv_word_t csr_addr = i_get_imm(instr);
    riscv_word_t old_csr = riscv_read_csr(riscv, csr_addr);
    riscv_word_t uimm = instr->i.rs1;
    riscv_word_t new_csr = old_csr & ~uimm;
    riscv_write_reg(riscv, instr->i.rd, old_csr);
    riscv_write_csr(riscv, csr_addr, new_csr); 
}

static void execute_MRET(riscv_t *riscv, instr_t *instr) {
    riscv_exit_irq(riscv);
}

static void exec_i_load_instrs(riscv_t *riscv, instr_t *instr) {
    riscv_word_t funct3 = instr->i.funct3;
    switch (funct3) {
    case FUNCT3_LB:
        execute_LB(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_LH:
        execute_LH(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_LW:
        execute_LW(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_LBU:
        execute_LBU(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_LHU:
        execute_LHU(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    default:
        break;
    }
}

static void exec_i_arith_shift_instrs(riscv_t *riscv, instr_t *instr) {
    riscv_word_t funct3 = instr->i.funct3;
    riscv_word_t imm7_0 = instr->i.imm_11_0 >> 5; 

    switch (funct3)
    {
    case FUNCT3_ADDI:
        execute_ADDI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_ORI:
        execute_ORI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_ANDI:
        execute_ANDI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_XORI:
        execute_XORI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SLTI:
        execute_SLTI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SLTIU:
        execute_SLTIU(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SLLI:
        execute_SLLI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SRLI_SRAI:
        switch (imm7_0) 
        {
        case IMM7_SRLI:
            execute_SRLI(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case IMM7_SRAI:
            execute_SRAI(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            break;
        }
        break;
    default:
        goto exception;
        break;
    }

    return;

    exception:
    return;
}

static void exec_r_instrs(riscv_t *riscv, instr_t *instr) {
    riscv_word_t funct3 = instr->r.funct3;
    riscv_word_t funct7 = instr->r.funct7;

    switch (funct3)
    {
    case FUNCT3_ADD_SUB_MUL:
        switch (funct7)
        {
        case FUNCT7_ADD:
            execute_ADD(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_SUB:
            execute_SUB(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_MUL:
            execute_MUL(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            goto exception;
            break;
        }
        break;
    case FUNCT3_OR_REM:
        switch (funct7) {
        case FUNCT7_OR:
            execute_OR(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_REM:
            execute_REM(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            break;
        }
        break;
    case FUNCT3_AND_REMU:
        switch (funct7) {
        case FUNCT7_AND:
            execute_AND(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_REMU:
            execute_REMU(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            break;
        }
        break;
    case FUNCT3_SLL_MULH:
        switch (funct7) {
        case FUNCT7_SLL:
            execute_SLL(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_MULH:
            execute_MULH(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            break;
        }
        break;
    case FUNCT3_SLT_MULHSU:
        switch (funct7) {
        case FUNCT7_SLT:
            execute_SLT(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_MULHSU:
            execute_MULHSU(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            break;
        }
        break;
    case FUNCT3_SLTU_MULU:
        switch (funct7) {
        case FUNCT7_SLTU:
            execute_SLTU(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_MULHU:
            execute_MULHU(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            break;
        }
        break;
    case FUNCT3_XOR_DIV:
        switch (funct7) {
        case FUNCT7_XOR:
            execute_XOR(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_DIV:
            execute_DIV(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            break;
        }
        break;
    case FUNCT3_SRL_SRA_DIVU:
        switch (funct7) 
        {
        case FUNCT7_SRL:
            execute_SRL(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_SRA:
            execute_SRA(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_DIVU:
            execute_DIVU(riscv, instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            break;
        }
        break;
    default:
        goto exception;
        break;
    }

    return;

    exception:
    return;
}

static void exec_s_instrs(riscv_t *riscv, instr_t *instr) {
    riscv_word_t funct3 = instr->s.funct3;
    switch (funct3) {
    case FUNCT3_SB:
        execute_SB(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SH:
        execute_SH(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SW:
        execute_SW(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    default:
        break;
    }
}

static void exec_b_instrs(riscv_t *riscv, instr_t *instr) {
    riscv_word_t funct3 = instr->b.funct3;
    switch (funct3) {
    case FUNCT3_BEQ:
        execute_BEQ(riscv, instr);
        break;
    case FUNCT3_BGE:
        execute_BGE(riscv, instr);
        break;
    case FUNCT3_BGEU:
        execute_BGEU(riscv, instr);
        break;
    case FUNCT3_BLT:
        execute_BLT(riscv, instr);
        break;
    case FUNCT3_BLTU:
        execute_BLTU(riscv, instr);
        break;
    case FUNCT3_BNE:
        execute_BNE(riscv, instr);
        break;
    default:
        break;
    }
}

static void exec_special_instrs(riscv_t *riscv, instr_t *instr) {
    riscv_word_t funct3 = instr->r.funct3;
    riscv_word_t funct7 = instr->r.funct7;
    switch (funct3) {
    case FUNCT3_EBREAK_MRET:
        switch (funct7) {
            case FUNCT7_EBREAK:
                break;
            case FUNCT7_MRET:
                execute_MRET(riscv, instr);
                break;
            default:
                break;
        }
        break;
    case FUNCT3_CSRRW:
        execute_CSRRW(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_CSRRS:
        execute_CSRRS(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_CSRRC:
        execute_CSRRC(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_CSRRWI:
        execute_CSRRWI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_CSRRSI:
        execute_CSRRSI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_CSRRCI:
        execute_CSRRCI(riscv, instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    default:
        break;
    }
}

int gdb_stop = 0;
int thread_stop = 0;

// thread should not exit after riscv_fetch_and_execute has finished
static void handle_gdb_stop_thread(void *arg) {
    gdb_server_t *server = (gdb_server_t *)arg;

    int timeout = 500;
    setsockopt(server->client, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    
    char ch = EOF;
    while (!thread_stop) {
        recv(server->client, &ch, 1, 0);
        if (ch == GDB_PAUSE) {
            gdb_stop = 1;
            break;
        }
    }

    timeout = 0;
    setsockopt(server->client, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
}

void riscv_fetch_and_execute(riscv_t *riscv, int forever) {
    device_t *flash_dev = &riscv->flash->device;
    if (riscv->pc < flash_dev->base || riscv->pc >= flash_dev->end) { // end is not valid address
        fprintf(stderr, "pc out of flash bound\n");
        return;
    }

    HANDLE handle;
    if (riscv->gdb_server && forever) {
        gdb_stop = 0;
        thread_stop = 0;
        handle = thread_create(handle_gdb_stop_thread, riscv->gdb_server);
    }
    
    do {
        if (riscv->pc >= flash_dev->end) {
            goto exception;
        }

        if (forever) {
            int detect = riscv_detect_breakpoint(riscv, riscv->pc);
            if (detect) {
                break;
            }
        }
        
        riscv_word_t *mem = (riscv_word_t *)riscv->flash->mem;
        riscv->instr.raw = mem[(riscv->pc - flash_dev->base) >> 2];

        switch(riscv->instr.opcode) {
            case OP_EBREAK_CSR:
                exec_special_instrs(riscv, &riscv->instr);
                if (riscv->instr.raw == EBREAK) {
                    goto ebreak;
                }
                break;
            case OP_LUI:
                execute_LUI(riscv, &riscv->instr);
                riscv->pc += sizeof(riscv_word_t);
                break;
            case OP_AUIPC:
                execute_AUIPC(riscv, &riscv->instr);
                riscv->pc += sizeof(riscv_word_t);
                break;
            case OP_JAL:
                execute_JAL(riscv, &riscv->instr);
                // riscv->pc += sizeof(riscv_word_t); // no need to add four for jal
                break;
            case OP_JALR:
                execute_JALR(riscv, &riscv->instr);
                // riscv->pc += sizeof(riscv_word_t); // no need to add four for jalr
                break;
            case OP_I_ARITH_SHIFT_INSTR:
                exec_i_arith_shift_instrs(riscv, &riscv->instr);
                break;
            case OP_I_LOAD_INSTR:
                exec_i_load_instrs(riscv, &riscv->instr);
                break;
            case OP_R_INSTR:
                exec_r_instrs(riscv, &riscv->instr);
                break;
            case OP_S_INSTR:
                exec_s_instrs(riscv, &riscv->instr);
                break;
            case OP_B_INSTR:
                exec_b_instrs(riscv, &riscv->instr);
                break;
            default:
                goto exception;
                break;
        }

        // is removing the pending after entering the handler a correct way?
        // no, since another interrupt might come and have higher priority
        if (riscv->csr_regs.mstatus & (1 << 3)) {
            int irq = pfic_get_irq_pending(riscv->pfic);
            if (irq >= 0 && irq != riscv->active_irq) {
                riscv_enter_irq(riscv, irq, riscv->pc, irq, 0);
            } 
        }

    } while (forever && !gdb_stop);

exception: 
ebreak:
    if (riscv->gdb_server && forever) {
        thread_stop = 1;
        thread_wait(handle);
    }
    return;
}

static device_t *riscv_find_device(riscv_t *riscv, riscv_word_t addr) {
    device_t *ret = (device_t*)0, *p = riscv->device_list;
    while (p) {
        if (addr < p->base || addr >= p->end) {
            p = p->next;
            continue;
        }
        ret = p;
        break;
    }

    return ret;
}

int riscv_mem_read(riscv_t *riscv, riscv_word_t addr, uint8_t *val, int width) {
    device_t *dev_read = riscv->dev_read;
    if (dev_read && addr >= dev_read->base && addr < dev_read->end) {
        return dev_read->read(dev_read, addr, val, width);
    } 
    
    device_t *device = riscv_find_device(riscv, addr);
    if (!device) {
        fprintf(stderr, "read at an invalid mem address, addr=%x\n", addr);
        return -1;
    }

    riscv->dev_read = device;
    return device->read(device, addr, val, width);
}

int riscv_mem_write(riscv_t *riscv, riscv_word_t addr, uint8_t *val, int width) {
    device_t *dev_write = riscv->dev_write;
    if (dev_write && addr >= dev_write->base && addr < dev_write->end) {
        return dev_write->write(dev_write, addr, val, width);
    } 
    
    device_t *device = riscv_find_device(riscv, addr);
    if (!device) {
        fprintf(stderr, "write at an invalid mem address, addr=%x\n", addr);
        return -1;
    }

    riscv->dev_write = device;
    return device->write(device, addr, val, width);
}

void riscv_run(riscv_t *riscv) {
    riscv_reset(riscv);

    if (riscv->gdb_server) {
        gdb_server_run(riscv->gdb_server);
        return;
    }
    riscv_fetch_and_execute(riscv, 1);
}

void riscv_add_breakpoint(riscv_t *riscv, riscv_word_t addr) {
    breakpoint_t *new = calloc(1, sizeof(breakpoint_t));
    new->addr = addr;
    new->next = riscv->bp_list;
    riscv->bp_list = new;
}

int riscv_remove_breakpoint(riscv_t *riscv, riscv_word_t addr) {
    breakpoint_t *pre = NULL;
    breakpoint_t *curr = riscv->bp_list;
    int remove = 0;
    while (curr) {
        if (curr->addr == addr) {
            remove = 1;
            if (pre) {
                pre->next = curr->next;
            } else {
                riscv->bp_list = curr->next;
            }
            break;
        }

        pre = curr;
        curr = curr->next;
    }

    return remove;
}

int riscv_detect_breakpoint(riscv_t *riscv, riscv_word_t addr) {
    breakpoint_t *curr = riscv->bp_list;
    while (curr) {
        if (curr->addr == addr) {
            return 1;
        }
        curr = curr->next;
    }

    return 0;
}

// set the csr regs and pc
// will enter the interrupt handler the next loop
void riscv_enter_irq(riscv_t *riscv, int irq, riscv_word_t mepc, riscv_word_t mcause, riscv_word_t mtval) {
    riscv->csr_regs.mepc = mepc;
    riscv->csr_regs.mcause = mcause;
    riscv->csr_regs.mtval = mtval;
    riscv->csr_regs.mstatus &= ~(1 << 7);
    riscv->csr_regs.mstatus |= (riscv->csr_regs.mstatus & (1 << 3)) << 4;
    
    riscv_word_t base = riscv->csr_regs.mtvec & 0xFFFFFFFC;
    riscv_word_t handler_saved_addr = base + irq * 4;
    
    riscv_mem_read(riscv, handler_saved_addr, (uint8_t*)&riscv->pc, sizeof(riscv_word_t));
    riscv->active_irq = irq;
}

// recover regs
// reset active irq
// reset pfic regs
void riscv_exit_irq(riscv_t *riscv) {
    riscv->pc = riscv->csr_regs.mepc;
    riscv->csr_regs.mstatus &= ~(1 << 3);
    riscv->csr_regs.mstatus |= (riscv->csr_regs.mstatus & (1 << 7)) >> 4;
    pfic_clear_irq_pending(riscv->pfic, riscv->active_irq);
    riscv->active_irq = 0;
}
