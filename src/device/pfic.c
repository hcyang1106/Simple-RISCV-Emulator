#include "device/pfic.h"
#include "stdlib.h"
#include <string.h>

device_t *pfic_create(const char *name, riscv_word_t base) {
    pfic_t *pfic = calloc(1, sizeof(pfic_t));
    device_init(&pfic->device, "pfic", 0, PFIC_BASE, sizeof(pfic_reg_t));
    pfic->device.read = pfic_read;
    pfic->device.write = pfic_write;
    return &pfic->device;
}

// does not include all of the registers
// only include regs used in test code
int pfic_read(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    pfic_t *pfic = (pfic_t*)device;
    riscv_word_t offset = addr - PFIC_BASE;
    if (offset >= 0 && offset <= 0x1C) {
        memcpy(&pfic->regs.ISR[offset >> 2], data, size);
    } else if (addr >= 0x20 && offset <= 0x3c) {
        memcpy(&pfic->regs.IPR[(offset - 0x20) >> 2], data, size);
    } else if (addr >= 0x400 && offset <= 0x43c) {
        // one byte each
        memcpy(&pfic->regs.IPRIOR[offset - 0x400], data, size); 
    } else {
        return -1;
    }

    return 0;
}

// does not include all of the registers
// only include regs used in test code
int pfic_write(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    pfic_t *pfic = (pfic_t*)device;
    riscv_word_t offset = addr - PFIC_BASE;
    if (offset >= 0x100 && offset <= 0x11C) {
        uint32_t set = 0;
        memcpy(&set, data, size);
        pfic->regs.ISR[(offset - 0x100) >> 2] |= set;
    } else if (addr >= 0x180 && offset <= 0x19C) {
        uint32_t mask = 0;
        memcpy(&mask, data, size);
        pfic->regs.ISR[(offset - 0x180) >> 2] &= ~mask;
    } else if ((offset >= 0x200) && (offset <= 0x21c)) {
        uint32_t set = 0;
        memcpy(&set, data, size);
        pfic->regs.IPR[(offset - 0x200) >> 2] |= set;
    } else if ((offset >= 0x280) && (offset <= 0x29c)) {
        uint32_t mask = 0;
        memcpy(&mask, data, size);
        pfic->regs.IPR[(offset - 0x280) >> 2] &= ~mask;
    } else if ((offset >= 0x400) && (offset <= 0x43c)) {
        memcpy(&pfic->regs.IPRIOR[offset - 0x400], data, size); 
    } else {
        return -1;
    }

    return 0;
}

int pfic_get_irq_pending(pfic_t *pfic) {
    int res_idx = -1;
    int res_prior = -1;

    for (int i = 0; i < 8; i++) {
        if (pfic->regs.IPR[i] == 0) {
            continue;
        }
        for (int j = 0; j < 32; j++) {
            if ((pfic->regs.IPR[i] & (1 << j)) == 0) {
                continue;
            }
            if ((pfic->regs.ISR[i] & (1 << j)) == 0) {
                continue;
            }

            // both enabled and pending
            int curr_idx = i*8 + j;
            int curr_prior = pfic->regs.IPRIOR[curr_idx];
            
            if (res_idx == -1) {
                res_idx = curr_idx;
                res_prior = curr_prior;
            } else {
                if (curr_prior < res_prior) {
                    res_idx = curr_idx;
                    res_prior = curr_prior;
                }
            }
        }
    }

    return res_idx;
}

void pfic_clear_irq_pending(pfic_t *pfic, int irq) {
    int reg_num = irq / 32;
    int in_reg_num = irq % 32;
    pfic->regs.IPR[reg_num] &= ~(1 << in_reg_num);
}

void pfic_set_irq_pending(pfic_t *pfic, int irq) {
    int reg_num = irq / 32;
    int in_reg_num = irq % 32;
    pfic->regs.IPR[reg_num] |= (1 << in_reg_num);
}