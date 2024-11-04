#include "device/usart.h"
#include <stdio.h>
#include <stdlib.h>
#include "device/mem.h"
#include <string.h>

device_t *usart_create(const char *name, riscv_word_t base) {
    usart_t *usart = calloc(1, sizeof(usart_t));

    // second arg of snprintf is usually the size of the first arg
    // snprintf(usart->device.name, sizeof(usart->device.name), "%s", name);
    device_init(&usart->device, name, 0, base, sizeof(usart_reg_t));
    usart->device.read = usart_read;
    usart->device.write = usart_write;

    return &usart->device;
}

int usart_read(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    riscv_word_t offset = addr - device->base;
    usart_t *usart = (usart_t*)device;

    switch (offset) {
        case USART1_STATR_OFF:
            break;
        case USART1_DATAR_OFF:
            break;
        case USART1_BRR_OFF:
            break;
        case USART1_CTRL_OFF:
            memcpy(data, (uint8_t*)&usart->regs.ctrl1, size);
            break;
        default:
            return -1;
            break;
    }

    return 0;
}

int usart_write(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    riscv_word_t offset = addr - device->base;
    usart_t *usart = (usart_t*)device;

    switch (offset) {
        case USART1_STATR_OFF:
            break;
        case USART1_DATAR_OFF:
            if (usart->regs.ctrl1 & (1 << 13)) {
                // ((usart_t*)device)->regs.datar = *(riscv_word_t*)data;
                putc(*data, stdout);
            }
            break;
        case USART1_BRR_OFF:
            break;
        case USART1_CTRL_OFF:
            memcpy((uint8_t*)&usart->regs.ctrl1, data, size);
            break;
        default:
            return -1;
            break;
    }

    return 0;
}

