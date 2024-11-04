#include <SDL.h>
#include <stdio.h>
#include "plat/plat.h"
#include "device/lcd.h"

static Uint32 user_event;

void thread_entry (void *arg) {
    lcd_t * lcd = (lcd_t *)arg;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        exit(-1);
    }

    // create window
    SDL_Window *window = SDL_CreateWindow(lcd->device.name,
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          lcd->width, lcd->height,
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
        exit(-1);
    }

    // create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                 SDL_RENDERER_ACCELERATED |
                                                 SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
        exit(-1);
    }

    // create texture (without filling frame buffer, just an empty texture)
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                            SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            lcd->width, lcd->height);
    if (!texture) {
        fprintf(stderr, "Failed to create SDL texture: %s\n", SDL_GetError());
        exit(-1);
    }

    // update texture (fill ing frame buffer)
    SDL_UpdateTexture(texture, NULL, lcd->frame_buf, lcd->width * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    user_event = SDL_RegisterEvents(1);

    SDL_Event event;
    int running = 1;
    int brush_radius = 8;
    int last_x = -1, last_y = -1;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == user_event) {
                SDL_UpdateTexture(texture, NULL, lcd->frame_buf, lcd->width * sizeof(uint32_t));
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);     
            } else if (event.type == SDL_MOUSEMOTION) {
                lcd->regs.mousex = event.motion.x;
                lcd->regs.mousey = event.motion.y;

                if (lcd->regs.mouse_st) {
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

                    if (last_x != -1 && last_y != -1) {
                        int dx = lcd->regs.mousex - last_x;
                        int dy = lcd->regs.mousey - last_y;
                        float distance = (float)sqrt(dx * dx + dy * dy);

                        for (float i = 0; i <= distance; i += 1.0f) {
                            float t = i / distance;
                            int intermediate_x = (int)(last_x + t * dx);
                            int intermediate_y = (int)(last_y + t * dy);

                            for (int w = -brush_radius; w <= brush_radius; w++) {
                                for (int h = -brush_radius; h <= brush_radius; h++) {
                                    if (w * w + h * h <= brush_radius * brush_radius) {
                                        SDL_RenderDrawPoint(renderer, intermediate_x + w, intermediate_y + h);
                                    }
                                }
                            }
                        }
                    }

                    last_x = lcd->regs.mousex;
                    last_y = lcd->regs.mousey;

                    for (int w = -brush_radius; w <= brush_radius; w++) {
                        for (int h = -brush_radius; h <= brush_radius; h++) {
                            if (w * w + h * h <= brush_radius * brush_radius) {
                                SDL_RenderDrawPoint(renderer, lcd->regs.mousex + w, lcd->regs.mousey + h);
                            }
                        }
                    }

                    SDL_RenderPresent(renderer);
                } else {
                    last_x = -1;
                    last_y = -1;
                }


            } else if (event.type == SDL_MOUSEBUTTONUP) {
                lcd->regs.mouse_st = 0;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                lcd->regs.mouse_st = 1;
            }
        }
        SDL_Delay(16);
    }    
}

device_t *lcd_create(const char *name, int width, int height) {
    lcd_t *lcd = calloc(1, sizeof(lcd_t));
    uint32_t *frame_buf = calloc(1, width * height * 4); // each pixel with 4 bytes
    lcd->frame_buf = frame_buf;
    device_init(&lcd->device, name, 0, LCD_BASE, LCD_BUF_BASE + width * height * 4 - LCD_BASE);
    lcd->width = width;
    lcd->height = height;
    lcd->device.read = lcd_read;
    lcd->device.write = lcd_write;
    thread_create(thread_entry, lcd);
    return &lcd->device;
}

int lcd_read(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    lcd_t *lcd = (lcd_t*)device;
    riscv_word_t offset = addr - LCD_BASE;
    switch (offset) {
        case LCD_CTRL_OFF: {
            memcpy(data, &lcd->regs.ctrl, size);
            break;
        }
        case LCD_MOUSEX_OFF: {
            memcpy(data, &lcd->regs.mousex, size);
            break;
        }
        case LCD_MOUSEY_OFF: {
            memcpy(data, &lcd->regs.mousey, size);
            break;
        }
        case LCD_MOUSE_ST_OFF: {
            memcpy(data, &lcd->regs.mouse_st, size);
            break;
        }
        default: {
            return -1;
            break;
        }
    }

    return 0;
}

int lcd_write(device_t *device, riscv_word_t addr, uint8_t *data, int size) {
    lcd_t *lcd = (lcd_t*)device;
    if (addr >= LCD_BUF_BASE) {
        memcpy(&lcd->frame_buf[(addr - LCD_BUF_BASE) / 4], data, size);
    } else {
        riscv_word_t val = 0;
        memcpy(&val, data, size);
        riscv_word_t offset = addr - LCD_BASE;
        switch (offset) {
            case LCD_CTRL_OFF:
                if (val & LCD_CTRL_FLUSH) {
                    SDL_Event event;
                    event.type = user_event;
                    SDL_PushEvent(&event); // this notifies the SDL polling thread
                }
                break;
            default:
                return -1;
                break;
        }
    }

    return 0;
}