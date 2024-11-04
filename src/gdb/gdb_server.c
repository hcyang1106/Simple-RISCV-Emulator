#include "core/riscv.h"
#include "gdb/gdb_server.h"
#include "plat/plat.h"
#include "string.h"
#include <stdint.h>
#include <time.h>

static char request[GDB_PACKET_SIZE];
static char checksum[GDB_CHECKSUM_SIZE+1];

gdb_server_t *gdb_server_create(struct _riscv_t *riscv, int port, int debug) {
    gdb_server_t *server = calloc(1, sizeof(gdb_server_t));
    RETURN_IF_MSG(server == NULL, err, "calloc gdb_server_t failed");

    socket_t fd = socket(AF_INET, SOCK_STREAM, 0); // IPV4, TCP
    RETURN_IF_MSG(fd == -1, err, strerror(errno));

    int reuse = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse));
    RETURN_IF_MSG(ret < 0, err, strerror(errno));

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY); // use any IP address available on machine
    sockaddr.sin_port = htons(port);
    ret = bind(fd, (struct sockaddr *) &sockaddr, sizeof(sockaddr));
    RETURN_IF_MSG(ret < 0, err, strerror(errno));

    ret = listen(fd, 1); // requests start to be queued
    RETURN_IF_MSG(ret < 0, err, strerror(errno));

    server->riscv = riscv;
    server->debug = debug;
    server->socket = fd;

    return server;
err:
    return (gdb_server_t*)0;
}

static void gdb_wait_client(gdb_server_t *server) {
    struct sockaddr_in sockaddr;
    int addr_len = sizeof(sockaddr);
    socket_t client = accept(server->socket, (struct sockaddr *)&sockaddr, &addr_len);
    RETURN_IF_MSG(client == INVALID_SOCKET, err, strerror(errno));
    server->client = client;
err:
    return;
}

static int buf2num(const char *buf) {
    int ret = 0;
    for (int i = 0; i < strlen(buf); i++) {
        if (buf[i] >= 'a' && buf[i] <= 'f') {
            ret = (ret << 4) + buf[i] - 'a' + 10;
        } else if (buf[i] >= 'A' && buf[i] <= 'F') {
            ret = (ret << 4) + buf[i] - 'A' + 10;
        } else {
            ret = (ret << 4) + buf[i] - '0';
        }
    }
    return ret;
}

// return the start of the message part
// validate using checksum
char *gdb_read_packet(gdb_server_t *server) {
    enum {
        INVALID,
        REQ_MSG_NORMAL,
        REQ_MSG_ESCAPE,
        CHECKSUM_0,
        CHECKSUM_1,
    }state = INVALID;

    char buf[1024];
    int req_i = 0, checksum_i = 0;
    int calc_val = 0;

    while (1) {
        int bytes_reveived = recv(server->client, buf, sizeof(buf)-1, 0); // last char should be \0
        int buf_i = 0;
        while (buf_i < bytes_reveived) {
            if (buf[buf_i] == EOF) {
                goto err; // connection closed
            } else if (state == INVALID) {
                if (buf[buf_i++] == '$') {
                    state = REQ_MSG_NORMAL;    
                }
            } else if (state == CHECKSUM_0) {
                RETURN_IF_MSG(checksum_i >= GDB_CHECKSUM_SIZE, err, "Checksum length exceeds maximum");
                checksum[checksum_i++] = buf[buf_i++];
                state = CHECKSUM_1;
            } else if (state == CHECKSUM_1) {
                RETURN_IF_MSG(checksum_i >= GDB_CHECKSUM_SIZE, err, "Checksum length exceeds maximum");
                checksum[checksum_i++] = buf[buf_i++];
                goto end_of_packet;
            } else if (state == REQ_MSG_NORMAL) { // other than '#'
                if (buf[buf_i] == '#') {
                    request[req_i++] = '\0';
                    buf_i++;
                    state = CHECKSUM_0;
                } else if (buf[buf_i] == '{') {
                    calc_val += '{';
                    buf_i++;
                    state = REQ_MSG_ESCAPE;
                } else {
                    RETURN_IF_MSG(req_i >= GDB_PACKET_SIZE, err, "Req message exceeds maximum");
                    request[req_i++] = buf[buf_i];
                    calc_val += buf[buf_i++];
                } 
            } else if (state == REQ_MSG_ESCAPE) { // other than '#'
                RETURN_IF_MSG(req_i >= GDB_PACKET_SIZE, err, "Req message exceeds maximum");
                request[req_i++] = buf[buf_i] ^ 0x20;
                calc_val += buf[buf_i++];
                state = REQ_MSG_NORMAL;
            }
        }
    }

end_of_packet:
    if (server->debug) {
        char buffer[80];
        time_t rawtime;
        time(&rawtime);
        struct tm * info = localtime(&rawtime);
        strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);
        fprintf(stdout, "%s <-$%s\n", buffer, request);
    }

    // TCP does checksum, so it is not required
    // checksum consists of two hex digits
    int check_val = buf2num(checksum);
    if (check_val != calc_val % 0x100) {
        if (server->debug) {
           fprintf(stderr, "->%s\n", request);
        }
        send(server->client, "-", 1, 0);
        RETURN_IF_MSG(1, err, "Checksum error");
    }

    send(server->client, "+", 1, 0); // success

    return request;
err:
    return NULL;
}

// pass in the msg and add checksum to form a packet
// send the packet
static int gdb_write_packet(gdb_server_t *server, const char *data) {
    char packet_buf[GDB_PACKET_SIZE];
    int idx = 0;
    packet_buf[idx++] = '$';
    uint8_t checksum = 0;
    const char *ptr = data;
    while (*ptr) {
        if (*ptr == '$' || *ptr == '*' || *ptr == '{' || *ptr == '#') {
            packet_buf[idx++] = '{';
            checksum += '{';
            packet_buf[idx++] = (*ptr) ^ 0x20;
        } else {
            packet_buf[idx++] = *ptr;
        }
        checksum += *ptr;
        ptr++;
    }

    sprintf(packet_buf + idx, "#%02x", checksum);
    send(server->client, packet_buf, (int)strlen(packet_buf), 0);

    if (server->debug) {
        char buffer[80];
        time_t rawtime;
        time(&rawtime);
        struct tm * info = localtime(&rawtime);
        strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);
        fprintf(stdout, "%s ->$%s\n", buffer, data);
    }

    char ch = EOF;
    recv(server->client, &ch, 1, 0);
    RETURN_IF_MSG(ch == EOF, err, "Connection closed");

    // we don't deal with the case when receive '-',
    // since tcp is reliable so there must be something wrong with our packet
    // therefore if we resend will get another '-' again
    return 0;
err:
    return -1;
}

static int gdb_write_unsupport(gdb_server_t *server) {
    // write an empty string
    return gdb_write_packet(server, "");
}

static int gdb_write_stop(gdb_server_t *server) {
    return gdb_write_packet(server, "S05");
}

static int gdb_handle_stop(gdb_server_t *server, const char *none) {
    return gdb_write_stop(server);
}

static int gdb_write_error(gdb_server_t *server, int code) {
    char rsp[GDB_PACKET_SIZE];
    sprintf(rsp, "E%02x", code);
    return gdb_write_packet(server, rsp);
}

static int gdb_handle_query(gdb_server_t *server, const char *query) {
    if (strncmp(query, "Supported", strlen("Supported")) == 0) {
        char rsp[GDB_PACKET_SIZE];
        snprintf(rsp, GDB_PACKET_SIZE, "PacketSize=%d", GDB_PACKET_SIZE);
        return gdb_write_packet(server, rsp);
    } else if (strncmp(query, "Attached", strlen("Attached")) == 0) {
        return gdb_write_packet(server, "1");
    } else if (strncmp(query, "Rcmd,726567", strlen("Rcmd,726567")) == 0) {
        riscv_reset(server->riscv);
        return gdb_write_packet(server, "OK");
    }

    return gdb_write_unsupport(server);
}

// can read any general reg or pc
static int gdb_handle_read_reg(gdb_server_t *server, const char *reg) {
    riscv_word_t reg_num = strtoul(reg, NULL, 16);

    // pc reg num is 32
    RETURN_IF_MSG(reg_num < 0 || reg_num > RISCV_REGS_NUM, err, "Invalid reg number");

    riscv_t *riscv = server->riscv;
    char rsp[GDB_PACKET_SIZE];
    char *write_ptr = rsp;
    riscv_word_t curr_reg = (reg_num == RISCV_REGS_NUM) ? riscv->pc : riscv_read_reg(riscv, reg_num);
    int bytes_per_reg = sizeof(riscv_word_t);
    for (int i = 0; i < bytes_per_reg; i++) { // 4 bytes
        int curr_byte = curr_reg & 0xFF;
        snprintf(write_ptr, 2+1, "%02x", curr_byte);
        curr_reg = curr_reg >> 8;
        write_ptr += 2;
    }

    return gdb_write_packet(server, rsp);

err:
    return gdb_write_error(server, GDB_ERROR_CODE);
}

// little endian
// only general regs
static int gdb_handle_read_regs(gdb_server_t *server, const char *none) {
    // 0x12345678 => 0x78453412
    riscv_t *riscv = server->riscv;
    char rsp[GDB_PACKET_SIZE];
    char *write_ptr = rsp;
    int bytes_per_reg = sizeof(riscv_word_t);
    for (int i = 0; i < RISCV_REGS_NUM; i++) {
        riscv_word_t curr_reg = riscv_read_reg(riscv, i);
        for (int j = 0; j < bytes_per_reg; j++) {
            int curr_byte = curr_reg & 0xFF;
            snprintf(write_ptr, 2+1, "%02x", curr_byte); // write one byte (2 hex digits)
            write_ptr += 2;
            curr_reg = curr_reg >> 8;
        }
    }

    return gdb_write_packet(server, rsp);
}

static int gdb_handle_read_mem(gdb_server_t *server, char *packet) {
    char *token = strtok(packet, ",");
    int addr = buf2num(token);
    
    token = strtok(NULL, ",");
    RETURN_IF_MSG(token == NULL, err, "Invalid read mem msg");
    int len = buf2num(token);

    char *buf = calloc(1, len);
    riscv_mem_read(server->riscv, addr, (uint8_t *)buf, len);

    char rsp[GDB_PACKET_SIZE];
    char *ptr = rsp;
    for (int i = 0; i < len; i++) {
        snprintf(ptr, 2+1, "%02x", (uint8_t)buf[i]);
        ptr += 2;
    }

    return gdb_write_packet(server, rsp);

err:
    return gdb_write_error(server, GDB_ERROR_CODE);
}

// gdb uses hex form to communicate, rather than in bytes
// AFEFCF => needs to convert into bytes => AF, EF, CF
static int gdb_handle_write_mem(gdb_server_t *server, char *packet) {
    char *token = strtok(packet, ",");
    int addr = buf2num(token);
    
    token = strtok(NULL, ":");
    RETURN_IF_MSG(token == NULL, err, "Invalid write mem msg");
    int len = buf2num(token);

    char *write_msg = strtok(NULL, ":");
    RETURN_IF_MSG(write_msg == NULL, err, "Invalid write mem msg");

    int i = 0;
    while (i < len) {
        char buf[2] = {*write_msg++, '\0'};
        uint8_t byte = buf2num(buf) << 4;
        buf[0] = *write_msg++;
        byte |= buf2num(buf);
        riscv_mem_write(server->riscv, addr + i, &byte, 1);
        i++;
    }

    return gdb_write_packet(server, "OK");

err:
    return gdb_write_error(server, GDB_ERROR_CODE);
}

static int gdb_handle_step(gdb_server_t *server, char *none) {
    riscv_fetch_and_execute(server->riscv, 0);
    return gdb_write_stop(server); // should not be something else
}

static int gdb_handle_continue(gdb_server_t *server, char *none) {
    // unsigned long unblock_recv = 1;
    // ioctlsocket(server->client, FIONBIO, (unsigned long *)&unblock_recv);
    riscv_fetch_and_execute(server->riscv, 1);
    // unblock_recv = 0;
    // ioctlsocket(server->client, FIONBIO, (unsigned long *)&unblock_recv);
    return gdb_write_stop(server); // should not be something else
}

static int gdb_handle_rm_breakpoint(gdb_server_t *server, char *packet) {
    char *token = strtok(packet, ",");
    token = strtok(NULL, ",");
    RETURN_IF_MSG(token == NULL, err, "No breakpoint address specified");
    riscv_word_t addr = buf2num(token);
    int ret = riscv_remove_breakpoint(server->riscv, addr);
    if (!ret) {
        goto err;
    }
    
    return gdb_write_packet(server, "OK");

err:
    return gdb_write_error(server, GDB_ERROR_CODE);
}

static int gdb_handle_add_breakpoint(gdb_server_t *server, char *packet) {
    char *token = strtok(packet, ",");
    token = strtok(NULL, ",");
    RETURN_IF_MSG(token == NULL, err, "No breakpoint address specified");
    riscv_word_t addr = buf2num(token);
    riscv_add_breakpoint(server->riscv, addr);
    return gdb_write_packet(server, "OK");

err:
    return gdb_write_error(server, GDB_ERROR_CODE);
}

// vmustreplyempty seems to be used to ensure a newer version of gdb, just reply empty
// Hg0 => set a certain thread
// trace => used to calc expressions at certain points without stopping a program
// SIGTRAP => os sends a SIGTRAP signal to program when encountering a breakpoint
static void gdb_handle_request(gdb_server_t *server) {
    char *packet;
    while ((packet = gdb_read_packet(server)) != NULL) {
        char ch = *packet++;
        switch (ch) {
            case 'q':
                gdb_handle_query(server, packet);
                break;
            case '?':
                gdb_handle_stop(server, packet);
                break;
            case 'g':
                gdb_handle_read_regs(server, packet);
                break;
            case 'p':
                gdb_handle_read_reg(server, packet);
                break;
            case 'm':
                gdb_handle_read_mem(server, packet);
                break;
            case 'M':
                gdb_handle_write_mem(server, packet);
                break;
            case 's':
                gdb_handle_step(server, packet);
                break;
            case 'c':
                gdb_handle_continue(server, packet);
                break;
            case 'z':
                gdb_handle_rm_breakpoint(server, packet);
                break;
            case 'Z':
                gdb_handle_add_breakpoint(server, packet);
                break;
            case 'k':
                return;
            default:
                gdb_write_unsupport(server);
                break;
        }
        
    }
}

static void gdb_close_connect(gdb_server_t *server) {
    closesocket(server->client);
}

void gdb_server_run(gdb_server_t *server) {
    while (1) {
        gdb_wait_client(server); // accept connection and create client socket
        gdb_handle_request(server); // calls gdb_read_packet, analyze the msg and calls gdb_write_packet
        gdb_close_connect(server); // close client socket
    }
}