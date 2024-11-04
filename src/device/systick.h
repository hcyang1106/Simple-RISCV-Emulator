#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>
#include "device/device.h"

#define SYSTICK_FREQ  100000000

#define SYSTICK_BASE    0xE000F000
#define SYSTICK_CTLR    0xE000F000
#define SYSTICK_SR      0xE000F004
#define SYSTICK_CNT     0xE000F008
#define SYSTICK_CMP     0xE000F010

typedef struct _systick_reg_t {
    uint32_t CTLR;
    uint32_t SR;
    uint64_t CNT; // the following two regs are 64 bits
    uint64_t CMP;
}systick_reg_t;

typedef struct _systick_t {
    device_t device;
    systick_reg_t regs;
}systick_t;

device_t *systick_create(const char * name, riscv_word_t base);
int systick_read(device_t *device, riscv_word_t addr, uint8_t *data, int size);
int systick_write(device_t *device, riscv_word_t addr, uint8_t *data, int size);

#endif