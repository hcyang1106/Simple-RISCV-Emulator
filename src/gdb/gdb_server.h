#ifndef GDB_SERVER_H
#define GDB_SERVER_H

#include "stdlib.h"
#include "stdio.h"
#include "plat/plat.h"

#define RETURN_IF_MSG(expr, err, msg) \
    if (expr) { \
        fprintf(stderr, "error: %s, file=%s, func=%s, line=%d\n", msg, __FILE__, __FUNCTION__, __LINE__); \
        goto err; \
    }

#define GDB_PACKET_SIZE (30 * 1024)
#define GDB_CHECKSUM_SIZE (2)
#define GDB_ERROR_CODE (1)
#define GDB_PAUSE (3)

struct _riscv_t;

typedef struct _gdb_server_t {
    struct _riscv_t *riscv;
    int debug;
    socket_t socket;
    socket_t client;
}gdb_server_t;

gdb_server_t *gdb_server_create(struct _riscv_t *riscv, int port, int debug);
void gdb_server_run(gdb_server_t *gdb_server);

#endif