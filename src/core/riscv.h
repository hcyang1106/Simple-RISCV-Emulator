#ifndef RISCV_H
#define RISCV_H

#include "types.h"
#include "device/mem.h"
#include "core/instr.h"
#include "gdb/gdb_server.h"
#include "device/pfic.h"

#define EI_NIDENT (16)
#define PT_LOAD 1       /* Loadable program segment */
#define ELFMAG0	0x7f    /* Magic number byte 0 */

typedef uint32_t Elf32_Word;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;

typedef struct
{
    unsigned char e_ident[EI_NIDENT];	/* Magic number and other info */
    Elf32_Half	e_type;			/* Object file type */
    Elf32_Half	e_machine;		/* Architecture */
    Elf32_Word	e_version;		/* Object file version */
    Elf32_Addr	e_entry;		/* Entry point virtual address */
    Elf32_Off	e_phoff;		/* Program header table file offset */
    Elf32_Off	e_shoff;		/* Section header table file offset */
    Elf32_Word	e_flags;		/* Processor-specific flags */
    Elf32_Half	e_ehsize;		/* ELF header size in bytes */
    Elf32_Half	e_phentsize;	/* Program header table entry size */
    Elf32_Half	e_phnum;		/* Program header table entry count */
    Elf32_Half	e_shentsize;	/* Section header table entry size */
    Elf32_Half	e_shnum;		/* Section header table entry count */
    Elf32_Half	e_shstrndx;		/* Section header string table index */
} Elf32_Ehdr;

typedef struct
{
    Elf32_Word	p_type;			/* Segment type */
    Elf32_Off	p_offset;		/* Segment file offset */
    Elf32_Addr	p_vaddr;		/* Segment virtual address */
    Elf32_Addr	p_paddr;		/* Segment physical address */
    Elf32_Word	p_filesz;		/* Segment size in file */
    Elf32_Word	p_memsz;		/* Segment size in memory */
    Elf32_Word	p_flags;		/* Segment flags */
    Elf32_Word	p_align;		/* Segment alignment */
} Elf32_Phdr;

#define RISCV_REGS_NUM 32

#define CSR_MARCHID         0xF12
#define CSR_MPIDID          0xF13
#define CSR_MSTATUS         0x300
#define CSR_MISA            0x301
#define CSR_MTVEC           0x305
#define CSR_MSCRATCH        0x340
#define CSR_MEPC            0x341
#define CSR_MCAUSE          0x342
#define CSR_MTVAL           0x343

typedef struct _csr_regs_t {
    riscv_word_t marchid;
    riscv_word_t mimpid;
    riscv_word_t mstatus;
    riscv_word_t mtvec;
    riscv_word_t mscratch;
    riscv_word_t mepc;
    riscv_word_t mcause;
    riscv_word_t mtval;
}csr_regs_t;

typedef struct _breakpoint_t {
    riscv_word_t addr;
    struct _breakpoint_t *next;
}breakpoint_t;

typedef struct _riscv_t {
    gdb_server_t *gdb_server;
    mem_t *flash;
    pfic_t *pfic;
    riscv_word_t regs[RISCV_REGS_NUM];
    riscv_word_t pc;
    instr_t instr;
    device_t *device_list;
    device_t *dev_read;
    device_t *dev_write;
    csr_regs_t csr_regs;
    breakpoint_t *bp_list;
    int active_irq;
}riscv_t;

#define riscv_read_reg(riscv, reg) (riscv->regs[reg])
#define riscv_write_reg(riscv, reg, val) if ((reg) != 0) {riscv->regs[(reg)] = (val);}

riscv_t *riscv_create(void);
void riscv_set_flash(riscv_t *riscv, mem_t *flash);
void riscv_set_pfic(riscv_t *riscv, pfic_t *pfic);
void riscv_load_bin(riscv_t *riscv, const char *path);
void riscv_load_elf(riscv_t *riscv, const char *path);
void riscv_continue(riscv_t *riscv, int forever);
void riscv_fetch_and_execute(riscv_t *riscv, int forever);
void riscv_reset(riscv_t *riscv);
void riscv_csr_init(riscv_t *riscv);
riscv_word_t riscv_read_csr(riscv_t *riscv, riscv_word_t addr);
void riscv_write_csr(riscv_t *riscv, riscv_word_t addr, riscv_word_t val);
int riscv_mem_read(riscv_t *riscv, riscv_word_t addr, uint8_t *val, int width);
int riscv_mem_write(riscv_t *riscv, riscv_word_t addr, uint8_t *val, int width);
void riscv_add_device(riscv_t *riscv, device_t *device);
void riscv_run(riscv_t *riscv);
void riscv_add_breakpoint(riscv_t *riscv, riscv_word_t addr);
int riscv_remove_breakpoint(riscv_t *riscv, riscv_word_t addr);
int riscv_detect_breakpoint(riscv_t *riscv, riscv_word_t addr);
void riscv_enter_irq(riscv_t *riscv, int irq, riscv_word_t mepc, riscv_word_t mcause, riscv_word_t mtval);
void riscv_exit_irq(riscv_t *riscv);

#endif