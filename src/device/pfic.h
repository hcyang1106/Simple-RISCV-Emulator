#ifndef PFIC_H
#define PFIC_H

#include <stdint.h>
#include "core/types.h"
#include "device/device.h"

#define PFIC_BASE 0xE000E000

#define IRQ_SWI 14
#define IRQ_SYSTICK 12

#pragma pack(1)
typedef struct _pfic_reg_t{
    riscv_word_t ISR[8];
    riscv_word_t IPR[8];
    uint32_t ITHRESDR;
    uint32_t RESERVED;
    uint32_t CFGR;
    riscv_word_t GISR;
    uint8_t VTFIDR[4];
    uint8_t RESERVED0[12];
    uint32_t VTFADDR[4];
    uint8_t RESERVED1[0x90];
    uint32_t IENR[8];
    uint8_t RESERVED2[0x60];
    uint32_t IRER[8];
    uint8_t RESERVED3[0x60];
    uint32_t IPSR[8];
    uint8_t RESERVED4[0x60];
    uint32_t IPRR[8];
    uint8_t RESERVED5[0x60];
    uint32_t IACTR[8];
    uint8_t RESERVED6[0xE0];
    uint8_t IPRIOR[256];
    uint8_t RESERVED7[0x810];
    uint32_t SCTLR;
}pfic_reg_t;
#pragma pack()

typedef struct _pfic_t {
    device_t device;
    pfic_reg_t regs;
}pfic_t;

int pfic_read(device_t *device, riscv_word_t addr, uint8_t *data, int size);
int pfic_write(device_t *device, riscv_word_t addr, uint8_t *data, int size);
device_t *pfic_create(const char *name, riscv_word_t base);

int pfic_get_irq_pending(pfic_t *pfic);
void pfic_clear_irq_pending(pfic_t *pfic, int irq);
void pfic_set_irq_pending(pfic_t *pfic, int irq);

#endif
