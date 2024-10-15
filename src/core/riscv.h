#ifndef RISCV_H
#define RISCV_H

#include "types.h"
#include "device/mem.h"
#include "core/instr.h"

#define RISCV_REGS_NUM 32

typedef struct _riscv_t {
    mem_t *flash;
    riscv_word_t regs[RISCV_REGS_NUM];
    riscv_word_t pc;
    instr_t instr;
    device_t *device_list;
    device_t *dev_read;
    device_t *dev_write;
}riscv_t;

#define riscv_read_reg(riscv, reg) (riscv->regs[reg])
#define riscv_write_reg(riscv, reg, val) if ((reg) != 0) {riscv->regs[(reg)] = (val);}

riscv_t *riscv_create(void);
void riscv_set_flash(riscv_t *riscv, mem_t *flash);
void riscv_load_bin(riscv_t *riscv, const char *path);
void riscv_continue(riscv_t *riscv, int forever);
void fetch_and_execute(riscv_t *riscv, int forever);
void riscv_reset(riscv_t *riscv);
int riscv_mem_read(riscv_t *riscv, riscv_word_t addr, uint8_t *val, int width);
int riscv_mem_write(riscv_t *riscv, riscv_word_t addr, uint8_t *val, int width);
void riscv_add_device(riscv_t *riscv, device_t *device);

#endif