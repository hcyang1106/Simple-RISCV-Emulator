#ifndef INSTR_H
#define INSTR_H

#include "core/types.h"
#include "core/riscv.h"

#define EBREAK 0b00000000000100000000000001110011

#define OP_EBREAK   0b1110011
#define OP_LUI     0b0110111
#define OP_AUIPC   0b0010111
#define OP_JAL     0b1101111
#define OP_JALR    0b1100111
#define OP_BEQ     0b1100011
#define OP_BNE     0b1100011
#define OP_BLT     0b1100011
#define OP_BGE     0b1100011
#define OP_BLTU    0b1100011
#define OP_BGEU    0b1100011

#define OP_I_LOAD_INSTR 0b0000011
// #define OP_LB      0b0000011
// #define OP_LH      0b0000011
// #define OP_LW      0b0000011
// #define OP_LBU     0b0000011
// #define OP_LHU     0b0000011

#define OP_S_INSTR 0b0100011
// #define OP_SB      0b0100011
// #define OP_SH      0b0100011
// #define OP_SW      0b0100011

#define OP_I_OTHERS_INSTR 0b0010011
// #define OP_ADDI    0b0010011
// #define OP_SLTI    0b0010011
// #define OP_SLTIU   0b0010011
// #define OP_XORI    0b0010011
// #define OP_ORI     0b0010011
// #define OP_ANDI    0b0010011
// #define OP_SLLI    0b0010011
// #define OP_SRLI    0b0010011
// #define OP_SRAI    0b0010011

#define OP_R_INSTR 0b0110011
// #define OP_ADD     0b0110011
// #define OP_SUB     0b0110011
// #define OP_SLL     0b0110011
// #define OP_SLT     0b0110011
// #define OP_SLTU    0b0110011
// #define OP_XOR     0b0110011
// #define OP_SRL     0b0110011
// #define OP_SRA     0b0110011
// #define OP_OR      0b0110011
// #define OP_AND     0b0110011

#define OP_NOP     0b0010011
#define OP_CSR     0b1110011

#define FUNCT3_ADDI      0b000
#define FUNCT3_SLTI      0b010
#define FUNCT3_SLTIU     0b011
#define FUNCT3_XORI      0b100
#define FUNCT3_ORI       0b110
#define FUNCT3_ANDI      0b111
#define FUNCT3_SLLI      0b001
#define FUNCT3_SRLI_SRAI 0b101

#define FUNCT3_SB        0b000
#define FUNCT3_SH        0b001
#define FUNCT3_SW        0b010

#define FUNCT3_LB        0b000
#define FUNCT3_LH        0b001
#define FUNCT3_LW        0b010
#define FUNCT3_LBU       0b100
#define FUNCT3_LHU       0b101

#define FUNCT3_ADD_SUB   0b000
// #define FUNCT3_ADD       0b000
// #define FUNCT3_SUB       0b000
#define FUNCT3_SLL       0b001
#define FUNCT3_SLT       0b010
#define FUNCT3_SLTU      0b011
#define FUNCT3_XOR       0b100
#define FUNCT3_SRL_SRA   0b101
// #define FUNCT3_SRL       0b101
// #define FUNCT3_SRA       0b101
#define FUNCT3_OR        0b110
#define FUNCT3_AND       0b111

#define FUNCT3_BEQ       0b000
#define FUNCT3_BNE       0b001
#define FUNCT3_BLT       0b100
#define FUNCT3_BGE       0b101
#define FUNCT3_BLTU      0b110
#define FUNCT3_BGEU      0b111

#define FUNCT3_MUL       0b000
#define FUNCT3_MULH      0b001
#define FUNCT3_MULHSU    0b010
#define FUNCT3_MULHU     0b011
#define FUNCT3_DIV       0b100
#define FUNCT3_DIVU      0b101
#define FUNCT3_REM       0b110
#define FUNCT3_REMU      0b111

#define FUNCT7_ADD       0b0000000
#define FUNCT7_SUB       0b0100000
#define FUNCT7_SRL       0b0000000
#define FUNCT7_SRA       0b0100000
#define FUNCT7_DIV       0b0000001
#define FUNCT7_MUL       0b0000001
#define FUNCT7_REM       0b0000001
#define FUNCT7_MRET      0b0011000

#define IMM7_SRLI 0b0000000
#define IMM7_SRAI 0b0100000

typedef union _instr_t {
    struct {
        riscv_word_t opcode: 7;
        riscv_word_t other: 25;
    }; 

    struct {
        riscv_word_t opcode: 7;
        riscv_word_t rd: 5;
        riscv_word_t funct3: 3;
        riscv_word_t rs1: 5;
        riscv_word_t rs2: 5;
        riscv_word_t funct7: 7;  
    }r;

    struct {
        riscv_word_t opcode: 7;
        riscv_word_t rd: 5;
        riscv_word_t funct3: 3;
        riscv_word_t rs1: 5;
        riscv_word_t imm_11_0: 12;
    }i;

    struct {
        riscv_word_t opcode: 7;
        riscv_word_t imm_4_0: 5;
        riscv_word_t funct3: 3;
        riscv_word_t rs1: 5;
        riscv_word_t rs2: 5;
        riscv_word_t imm_11_5: 7;
    }s;

    struct {
        riscv_word_t opcode: 7;
        riscv_word_t imm_11: 1;
        riscv_word_t imm_4_1: 4;
        riscv_word_t funct3: 3;
        riscv_word_t rs1: 5;
        riscv_word_t rs2: 5;
        riscv_word_t imm_10_5: 6;
        riscv_word_t imm_12: 1;
    }b;

    struct {
        riscv_word_t opcode: 7;
        riscv_word_t rd: 5;
        riscv_word_t imm_31_12 : 20;
    }u;

    struct {
        riscv_word_t opcode: 7;
        riscv_word_t rd: 5;
        riscv_word_t imm_19_12 : 8;
        riscv_word_t imm_11 : 1;
        riscv_word_t imm_10_1 : 10;
        riscv_word_t imm_20 : 1;
    }j;

    riscv_word_t raw;
}instr_t;

#endif