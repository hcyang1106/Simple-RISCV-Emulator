#include <string.h>
#include "plat/plat.h"
#include "core/riscv.h"
#include "device/mem.h"
#include "test/instr_test.h"
// #include "test.h"       

#define RISCV_FLASH_BASE 0
#define RISCV_FLASH_SIZE (16 * 1024 * 1024)

#define RISCV_RAM_BASE 0x20000000
#define RISCV_RAM_SIZE (16 * 1024 * 1024)

int main(int argc, char** argv) {
    plat_init();

    riscv_t *riscv = riscv_create();
    mem_t *ram = mem_create("ram", MEM_ATTR_READABLE | MEM_ATTR_WRITABLE, RISCV_RAM_BASE, RISCV_RAM_SIZE);
    riscv_add_device(riscv, &ram->device);

    mem_t *flash = mem_create("flash", MEM_ATTR_READABLE, RISCV_FLASH_BASE, RISCV_FLASH_SIZE);
    riscv_add_device(riscv, &flash->device);
    riscv_set_flash(riscv, flash);
    // the path is relative to "current work directory"
    riscv_load_bin(riscv, "./unit/ebreak/obj/image.bin");

    instr_test(riscv);
    // test_env();
    return 0;
}
