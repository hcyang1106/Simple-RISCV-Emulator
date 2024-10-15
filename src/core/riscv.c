#include "core/riscv.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/instr.h"
#include "device/device.h"

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
}

void riscv_set_flash(riscv_t *riscv, mem_t *flash) {
    riscv->flash = flash;
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

    while (fread(riscv->flash->mem, 1, 1024, file) > 0) {}
    
    fclose(file);
}

void riscv_reset(riscv_t *riscv) {
    riscv->pc = 0;
    riscv->instr.raw = 0;
    riscv->dev_read = riscv->dev_write = (device_t *)0;
    memset(riscv->regs, 0, sizeof(riscv->regs));
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

static void execute_LUI(riscv_t *riscv, instr_t *instr) {
    int32_t imm = u_get_imm(instr);
    riscv_write_reg(riscv, instr->u.rd, imm); // if rd invalid?
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

static void execute_LB(riscv_t *riscv, instr_t *instr) {
    riscv_word_t rs1_val = riscv_read_reg(riscv, instr->i.rs1);
    riscv_word_t addr = rs1_val + i_get_imm(instr);
    riscv_word_t byte;
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

static void exec_i_others_instrs(riscv_t *riscv, instr_t *instr) {
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
    case FUNCT3_ADD_SUB:
        switch (funct7)
        {
        case FUNCT7_ADD:
            execute_ADD(riscv, &riscv->instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        case FUNCT7_SUB:
            execute_SUB(riscv, &riscv->instr);
            riscv->pc += sizeof(riscv_word_t);
            break;
        default:
            goto exception;
            break;
        }
        break;
    case FUNCT3_OR:
        execute_OR(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_AND:
        execute_AND(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SLL:
        execute_SLL(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SLT:
        execute_SLT(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SLTU:
        execute_SLTU(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_XOR:
        execute_XOR(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SRL_SRA:
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

static void execute_s_instrs(riscv_t *riscv, instr_t *instr) {
    riscv_word_t funct3 = instr->s.funct3;
    switch (funct3) {
    case FUNCT3_SB:
        execute_SB(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SH:
        execute_SH(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    case FUNCT3_SW:
        execute_SW(riscv, &riscv->instr);
        riscv->pc += sizeof(riscv_word_t);
        break;
    default:
        break;
    }
}

void riscv_continue(riscv_t *riscv, int forever) {
}

void fetch_and_execute(riscv_t *riscv, int forever) {
    device_t *flash_dev = &riscv->flash->device;
    if (riscv->pc < flash_dev->base || riscv->pc >= flash_dev->end) { // end is not valid address
        fprintf(stderr, "pc out of flash bound\n");
        return;
    }

    do {
        if (riscv->pc >= flash_dev->end) {
            goto exception;
        }

        riscv_word_t *mem = (riscv_word_t *)riscv->flash->mem;
        riscv->instr.raw = mem[(riscv->pc - flash_dev->base) >> 2];

        switch(riscv->instr.opcode) {
            case OP_EBREAK:
                execute_EBREAK(riscv, &riscv->instr);
                break;
            case OP_LUI:
                execute_LUI(riscv, &riscv->instr);
                riscv->pc += sizeof(riscv_word_t);
                break;
            case OP_I_OTHERS_INSTR:
                exec_i_others_instrs(riscv, &riscv->instr);
                break;
            case OP_I_LOAD_INSTR:
                exec_i_load_instrs(riscv, &riscv->instr);
                break;
            case OP_R_INSTR:
                exec_r_instrs(riscv, &riscv->instr);
                break;
            case OP_S_INSTR:
                execute_s_instrs(riscv, &riscv->instr);
                break;
            default:
                goto exception;
                break;
        }
    } while (forever);

    return;

    exception: // exception happens
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