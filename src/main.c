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

#define GDB_SERVER_DEFAULT_PORT 3333

static void print_usage(const char *filename) {
    fprintf(stdout, "usage: %s [options] <elf file>\n"
                    "-h | print help\n"
                    "-t | unit test\n"
                    "-d | print gdb trace\n"
                    "-g [option] | enable gdb server"
                    "-r addr:size | set ram range\n"
                    "-f addr:size | set flash range\n", filename
    );
}

static int is_opt(const char *opts[], const char *opt, int len) {
    if (sizeof(opts) == 0) {
        return 0;
    }
    // this is a mistake here, opts is actually a pointer so sizeof doesn't behabe as expected
    // for (int i = 0; i < sizeof(opts)/sizeof(opt[0]); i++) {
    for (int i = 0; i < len; i++) {
        if (strncmp(opts[i], opt, strlen(opt)) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    plat_init();

    riscv_t *riscv = riscv_create();

    const char *opts[] = {"-h", "-t", "-g", "-r", "-f", "-d"};
    
    int has_ram = 0;
    int has_flash = 0;
    int is_run_test = 0;
    int is_debug = 0;
    int has_gdb_server = 0;
    int gdb_server_port = GDB_SERVER_DEFAULT_PORT;

    int i = 1;
    while (i < argc) {
        if (strncmp(argv[i], "-h", 2) == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strncmp(argv[i], "-r", 2) == 0) {
            if (i + 1 >= argc || is_opt(opts, argv[i+1], sizeof(opts)/sizeof(opts[0]))) { // without arg
                fprintf(stderr, "Please specify a range for ram\n");
                exit(0);
            }

            char *arg = argv[i+1];
            
            char *token = strtok(arg, ": ");
            // first time executing strtok does not go through this
            // if (!token) { 
            //     fprintf(stderr, "Please specify a range for ram\n");
            //     exit(0);
            // }

            char *end_ptr;
            unsigned long base = strtoul(token, &end_ptr, 16);
            if (end_ptr == token) {
                fprintf(stderr, "Please specify a range for ram\n");
                exit(0);
            }

            token = strtok(NULL, " :");
            if (!token) {
                fprintf(stderr, "Please specify a range for ram\n");
                exit(0);
            }

            unsigned long size = strtoul(token, &end_ptr, 16);
            if (end_ptr == token) {
                fprintf(stderr, "Please specify a range for ram\n");
                exit(0);
            }

            mem_t *ram = mem_create("ram", MEM_ATTR_READABLE | MEM_ATTR_WRITABLE, base, size);
            riscv_add_device(riscv, &ram->device);
            has_ram = 1;
            i++;
        } else if(strncmp(argv[i], "-f", 2) == 0) {
            if (i + 1 >= argc || is_opt(opts, argv[i+1], sizeof(opts)/sizeof(opts[0]))) { // without arg
                fprintf(stderr, "Please specify a range for flash\n");
                exit(0);
            }

            char *arg = argv[i+1];
            
            char *token = strtok(arg, ": ");
            char *end_ptr;
            unsigned long base = strtoul(token, &end_ptr, 16);
            if (end_ptr == token) {
                fprintf(stderr, "Please specify a range for flash\n");
                exit(0);
            }

            token = strtok(NULL, " :");
            if (!token) {
                fprintf(stderr, "Please specify a range for flash\n");
                exit(0);
            }

            unsigned long size = strtoul(token, &end_ptr, 16);
            if (end_ptr == token) {
                fprintf(stderr, "Please specify a range for flash\n");
                exit(0);
            }

            mem_t *flash = mem_create("flash", MEM_ATTR_READABLE | MEM_ATTR_WRITABLE, base, size);
            riscv_add_device(riscv, &flash->device);
            riscv_set_flash(riscv, flash);
            has_flash = 1;
            i++;
        } else if (strncmp(argv[i], "-t", 2) == 0) {
            is_run_test = 1;
        } else if (strncmp(argv[i], "-d", 2) == 0) {
            is_debug = 1;
        } else if (strncmp(argv[i], "-g", 2) == 0) {
            has_gdb_server = 1;
            if (i + 1 < argc && !is_opt(opts, argv[i+1], sizeof(opts)/sizeof(opts[0]))) { // specifies a port
                char *end_ptr;
                gdb_server_port = (int)strtoul(argv[i+1], &end_ptr, 10);
                if (end_ptr == argv[i+1]) {
                    fprintf(stderr, "Please specify a valid port\n");
                    exit(0);
                }
                i++;
            }
        } else {
            fprintf(stderr, "Unsupported option %s\n", argv[i]);
            exit(0);
        }

        i++;
    }

    if (!has_ram) {
        mem_t *ram = mem_create("ram", MEM_ATTR_READABLE | MEM_ATTR_WRITABLE, RISCV_RAM_BASE, RISCV_RAM_SIZE);
        riscv_add_device(riscv, &ram->device);
    }
    
    if (!has_flash) {
        mem_t *flash = mem_create("flash", MEM_ATTR_READABLE | MEM_ATTR_WRITABLE, RISCV_FLASH_BASE, RISCV_FLASH_SIZE);
        riscv_add_device(riscv, &flash->device);
        riscv_set_flash(riscv, flash);
    }
    
    if (is_run_test) {
        instr_test(riscv);
    }

    if (has_gdb_server) {
        gdb_server_t *server = gdb_server_create(riscv, gdb_server_port, is_debug);
        riscv->gdb_server = server;
        if (!server) {
            fprintf(stderr, "Create gdb server failed\n" );
            exit(0);
        }
        fprintf(stdout, "gdb server is running on port %d\n", gdb_server_port);
    }
    
    riscv_run(riscv);

    return 0;
}
