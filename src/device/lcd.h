#ifndef LCD_H
#define LCD_H

#include "device/device.h"

#define LCD_BASE            0xA0000000 // start of address of regs
#define LCD_BUF_BASE        0xA1000000 // start of frame buffer

#define LCD_CTRL_OFF        0 // reg offsets from base 
#define LCD_MOUSEX_OFF      4
#define LCD_MOUSEY_OFF      8
#define LCD_MOUSE_ST_OFF    12

#define LCD_CTRL_FLUSH      (1 << 0) // flush frame buffer

typedef struct _lcd_reg_t {
    uint32_t ctrl;
    uint32_t mousex;
    uint32_t mousey;
    uint32_t mouse_st;
}lcd_reg_t;

typedef struct _LCD_t {
    device_t device;
    uint32_t *frame_buf;
    int width, height;
    lcd_reg_t regs;
}lcd_t;

device_t *lcd_create(const char *name, int width, int height);
int lcd_read(device_t *device, riscv_word_t addr, uint8_t *data, int size);
int lcd_write(device_t *device, riscv_word_t addr, uint8_t *data, int size);

#endif 