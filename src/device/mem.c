#include "device/mem.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

mem_t *mem_create(const char *name, riscv_word_t attr, riscv_word_t base, riscv_word_t size) {
    mem_t *mem = calloc(1, sizeof(mem_t));
    if (mem == NULL) {
        fprintf(stderr, "memory alloc failed\n");
        return mem;
    }

    mem->mem = malloc(size);
    if (mem->mem == NULL) {
        fprintf(stderr, "memory alloc failed\n");
        return NULL;
    }

    device_t *device = &mem->device;
    device_init(device, name, attr, base, size);
    device->read = mem_read;
    device->write = mem_write;

    return mem;
}

int mem_read(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    if (!(device->attr & MEM_ATTR_READABLE)) {
        fprintf(stderr, "device %s not readable\n", device->name);
        return -1;
    }

    // device_t is the first attribute in mem_t
    mem_t *mem = (mem_t*)device;
    riscv_word_t offset = addr - device->base; // check valid?

    if (size == 1) {
        memcpy(data, mem->mem + offset, 1);
    } else if (size == 2) {
        memcpy(data, mem->mem + offset, 2);
    } else if (size == 4) {
        memcpy(data, mem->mem + offset, 4);
    } else {
        // generate exception?
    }

    return 0;
}

int mem_write(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    if (!(device->attr & MEM_ATTR_WRITABLE)) {
        fprintf(stderr, "device %s not writable\n", device->name);
        return -1;
    }

    // device_t is the first attribute in mem_t
    mem_t *mem = (mem_t*)device;
    riscv_word_t offset = addr - device->base; // check valid?

    if (size == 1) {
        memcpy(mem->mem + offset, data, 1);
    } else if (size == 2) {
        memcpy(mem->mem + offset, data, 2);
    } else if (size == 4) {
        memcpy(mem->mem + offset, data, 4);
    } else {
        // generate exception?
    }

    return 0;
}