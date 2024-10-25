#ifndef RISCV_H
#define RISCV_H

#include "types.h"
#include "device/mem.h"
#include "core/instr.h"
#include "gdb/gdb_server.h"

#define RISCV_REGS_NUM 32

#define CSR_MSCRATCH 0x340

typedef struct _breakpoint_t {
    riscv_word_t addr;
    struct _breakpoint_t *next;
}breakpoint_t;

typedef struct _csr_regs_t {
    riscv_word_t mscratch;
}csr_regs_t;

typedef struct _riscv_t {
    gdb_server_t *gdb_server;
    mem_t *flash;
    riscv_word_t regs[RISCV_REGS_NUM];
    riscv_word_t pc;
    instr_t instr;
    device_t *device_list;
    device_t *dev_read;
    device_t *dev_write;
    csr_regs_t csr_regs;
    breakpoint_t *bp_list;
}riscv_t;

#define riscv_read_reg(riscv, reg) (riscv->regs[reg])
#define riscv_write_reg(riscv, reg, val) if ((reg) != 0) {riscv->regs[(reg)] = (val);}

riscv_t *riscv_create(void);
void riscv_set_flash(riscv_t *riscv, mem_t *flash);
void riscv_load_bin(riscv_t *riscv, const char *path);
void riscv_continue(riscv_t *riscv, int forever);
void riscv_fetch_and_execute(riscv_t *riscv, int forever);
void riscv_reset(riscv_t *riscv);
void riscv_csr_init(riscv_t *riscv);
riscv_word_t riscv_read_csr(riscv_t *riscv, riscv_word_t addr);
void riscv_write_csr(riscv_t *riscv, riscv_word_t addr, riscv_word_t val);
int riscv_mem_read(riscv_t *riscv, riscv_word_t addr, uint8_t *val, int width);
int riscv_mem_write(riscv_t *riscv, riscv_word_t addr, uint8_t *val, int width);
void riscv_add_device(riscv_t *riscv, device_t *device);
void riscv_run(riscv_t *riscv);
void riscv_add_breakpoint(riscv_t *riscv, riscv_word_t addr);
int riscv_remove_breakpoint(riscv_t *riscv, riscv_word_t addr);
int riscv_detect_breakpoint(riscv_t *riscv, riscv_word_t addr);

#endif