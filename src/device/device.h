#ifndef DEVICE_H
#define DEVICE_H

#include "core/types.h"

struct _riscv_t; 
typedef struct _device_t {
    char name[64];
    riscv_word_t attr;
    riscv_word_t base;
    riscv_word_t end;

    struct _device_t *next;
    struct _riscv_t *riscv;
    int (*read)(struct _device_t *device, riscv_word_t addr, uint8_t *data, int size);
    int (*write)(struct _device_t *device, riscv_word_t addr, uint8_t *data, int size);
}device_t;

void device_init(device_t *device, const char *name, riscv_word_t attr,
    riscv_word_t base, riscv_word_t size);

#endif