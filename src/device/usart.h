#ifndef USART_H
#define USART_H

#include "device/device.h"
#include <stdint.h>

#define USART1_BASE         0x40013800
#define USART1_STATR_OFF    0
#define USART1_DATAR_OFF    4          
#define USART1_BRR_OFF      8 
#define USART1_CTRL_OFF     12

// does not include all of the registers
// only include regs used in test code
typedef struct _usart_reg_t {
    uint16_t statr;
    uint16_t reserved0;
    uint16_t datar;
    uint16_t reserved1;
    uint16_t brr;
    uint16_t reserved2;
    uint16_t ctrl1;
}usart_reg_t;

typedef struct _usart_t {
    device_t device; 
    usart_reg_t regs;
}usart_t;

device_t *usart_create(const char *name, riscv_word_t base);
int usart_read(device_t *device, riscv_word_t addr, uint8_t *data, int size);
int usart_write(device_t *device, riscv_word_t addr, uint8_t *data, int size);

#endif