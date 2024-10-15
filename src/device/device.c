#include "device/device.h"
#include "string.h"

void device_init(device_t *device, const char *name, riscv_word_t attr,
    riscv_word_t base, riscv_word_t size) {
    memcpy(device->name, name, sizeof(name));
    device->attr = attr;
    device->base = base;
    device->end = base + size;
}