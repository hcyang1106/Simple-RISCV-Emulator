#ifndef MEM_H
#define MEM_H

#include "core/types.h"
#include "device/device.h"

#define MEM_ATTR_READABLE     (1 << 0)
#define MEM_ATTR_WRITABLE     (1 << 1)

typedef struct _mem_t {
    device_t device;
    uint8_t *mem;
}mem_t;

mem_t *mem_create(const char *name, riscv_word_t attr, riscv_word_t base, riscv_word_t size);
int mem_read(device_t *device, riscv_word_t addr, uint8_t *data, int size);
int mem_write(device_t *device, riscv_word_t addr, uint8_t *data, int size);


#endif