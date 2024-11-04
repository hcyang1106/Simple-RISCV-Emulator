#include "device/systick.h"
#include "device/pfic.h"
#include "core/riscv.h"
#include "plat/plat.h"

static void systick_thread(void *arg) {
    systick_t *systick = (systick_t *)arg;
    while (1) {
        if ((systick->regs.CTLR & (1 << 0)) == 0) { // start systick
            continue;
        }

        thread_msleep((int)(systick->regs.CMP / (SYSTICK_FREQ / 1000)));
        
        systick->regs.SR |= 1;
        if (systick->regs.CTLR & (1 << 1)) {
            pfic_set_irq_pending(systick->device.riscv->pfic, IRQ_SYSTICK);
        }
    }
}

device_t *systick_create(const char * name, riscv_word_t base) {
    systick_t *systick = calloc(1, sizeof(systick_t));
    device_init(&systick->device, "systick", 0, base, sizeof(systick_reg_t));
    systick->device.read = systick_read;
    systick->device.write = systick_write;
    thread_create(systick_thread, systick);
    return &systick->device;
}

int systick_read(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    systick_t *systick = (systick_t *)device;
    switch (addr) {
        case SYSTICK_CTLR:
            memcpy(data, &systick->regs.CTLR, size);
            break;
        case SYSTICK_SR:
            memcpy(data, &systick->regs.SR, size);
            break;
        case SYSTICK_CNT:
            memcpy(data, &systick->regs.CNT, size);
            break;
        case SYSTICK_CNT + 4: // since the reg is 64 bits long, it needs to read twice
            memcpy(data, &systick->regs.CNT + 1, size);
            break;
        case SYSTICK_CMP:
            memcpy(data, &systick->regs.CMP, size);
            break;
        case SYSTICK_CMP + 4: // since the reg is 64 bits long, it needs to read twice
            memcpy(data, &systick->regs.CMP + 1, size);
            break;
        default:
            return -1;
            break;
    }

    return 0;
}

int systick_write(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    systick_t *systick = (systick_t*)device;
    switch (addr) {
        case SYSTICK_SR: // initial value is 0, set to 1 after interrupt triggers
            riscv_word_t val = 0;
            memcpy(&val, data, size);
            if ((val & 0x1) == 0) {
                systick->regs.SR = 0;
            }
            break;
        case SYSTICK_CTLR:
            val = 0;
            memcpy(&val, data, size);
            systick->regs.CTLR = val;

            // CNT reg is set but is actually not used
            if (val & (1 << 4) && val & (1 << 5)) { // from high to low
                systick->regs.CNT = systick->regs.CMP;
            } else if (~(val & (1 << 4)) && val & (1 << 5)) { // from low to high
                systick->regs.CNT = 0;
            }
            // triggers SWI interrupt
            if (val & (1 << 31)) {
                pfic_set_irq_pending(systick->device.riscv->pfic, IRQ_SWI);
            }
            break;
        case SYSTICK_CNT:
            memcpy(&systick->regs.CNT, data, size);
            break;
        case SYSTICK_CNT + 4: // since the reg is 64 bits long, it needs to read twice
            memcpy(&systick->regs.CNT + 1, data, size);
            break;
        case SYSTICK_CMP:
            memcpy(&systick->regs.CMP, data, size);
            break;
        case SYSTICK_CMP + 4: // since the reg is 64 bits long, it needs to read twice
            memcpy(&systick->regs.CMP + 1, data , size);
            break;
        default:
            return -1;
            break;
    }

    return 0;
}