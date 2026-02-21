#pragma once
#include "common.h"

//* file system macro and struct

#define FILES_MAX 2
#define DISK_MAX_SIZE align_up(sizeof(struct file) * FILES_MAX, SECTOR_SIZE)

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
    char data[]; // array pointing to the data area following the header. È un array a lunghezza variabile.
}__attribute__((packed));

struct file {
    bool in_use;    // indica se il file entry è in uso o meno
    char name[100]; // file name
    char data[1024]; // contenuto del file
    size_t size; // dimensione del file
};



//* disk I/O macro and struct

#define SECTOR_SIZE 512
#define VIRTQ_ENTRY_NUM 16
#define VIRTIO_DEVICE_BLK 2
#define VIRTIO_BLK_PADDR 0x10001000
#define VIRTIO_REG_MAGIC 0x00
#define VIRTIO_REG_VERSION 0x04
#define VIRTIO_REG_DEVICE_ID 0x08
#define VIRTIO_REG_PAGE_SIZE 0x28
#define VIRTIO_REG_QUEUE_SEL 0x30
#define VIRTIO_REG_QUEUE_NUM_MAX 0x34
#define VIRTIO_REG_QUEUE_NUM 0x38
#define VIRTIO_REG_QUEUE_PFN 0x40
#define VIRTIO_REG_QUEUE_READY 0x44
#define VIRTIO_REG_QUEUE_NOTIFY 0x50
#define VIRTIO_REG_DEVICE_STATUS 0x70
#define VIRTIO_REG_DEVICE_CONFIG 0x100
#define VIRTIO_STATUS_ACK 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1

// virtqueue Descriptor table entry
struct virtq_desc{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
}__attribute__((packed));

// virtqueue Avaiable Ring
struct virtq_avail{
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM];
}__attribute__((packed));

// virtqueue Used Ring entry
struct virtq_used_elem{
    uint32_t id;
    uint32_t len;
}__attribute__((packed));

// virtqueue used ring
struct virtq_used{
    uint16_t flags;
    uint16_t index;
    struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
}__attribute__((packed));

// Virtqueue

struct virtio_virtq{
    struct virtq_desc descs[VIRTQ_ENTRY_NUM];
    struct virtq_avail avail;
    struct virtq_used used __attribute__((aligned(PAGE_SIZE)));
    int queue_index;
    volatile uint16_t *used_index;
    uint16_t last_used_index;
}__attribute__((packed));

// Virtio-blk request
struct virtio_blk_req {
    // primo descrittore: read-only from the device 
    uint32_t type;
    uint32_t reserved; // PADDING (spazio vuoto richiesto dallo standard)
    uint64_t sector;
    
    // secondo descrittore writable by the device if it's a read operation (VIRTQ_DESC_F_WRITE)
    uint8_t data[512];
    
    // terzo descrittore: writable by the device (VIRTQ_DESC_F_WRITE)
    uint8_t status;
}__attribute__((packed));






//* syscall

#define SCAUSE_ECALL 8


//* User Mode

// l'indirizzo virtuale dell'immagine dell'applicazione.
// Tale indirizzo deve coincidere con quello definito in `user.ld`.

#define USER_BASE 0x10000000
#define SSTATUS_SPIE (1 << 5)
#define SSTATUS_SUM (1 << 18)

extern char _binary_shell_bin_start[], _binary_shell_bin_size[];


//* macro e PCB per i processi

#define PROCS_MAX 8

#define PROC_UNUSED 0   // PCB non utilizzata
#define PROC_RUNNABLE 1 // PCB utilizzata ed eseguibile

#define PROC_EXITED 2

struct process {
    int pid;
    int state;
    vaddr_t sp; // stack pointer → indirizzo virtuale
    uint32_t *page_table;
    uint8_t stack[8192]; // kernel stack
};


//* contenente i 31 indirizzi che abbiamo memorizzato nello stack
struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define READ_CSR(reg)                                                          \
    ({                                                                         \
        unsigned long __tmp;                                                   \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp));                  \
        __tmp;                                                                 \
    })

#define WRITE_CSR(reg, value)                                                  \
    do {                                                                       \
        uint32_t __tmp = (value);                                              \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp));                \
    } while (0)


#define PANIC(fmt, ...) \
	do { \
		printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
		while (1) {} \
	} while (0) \

typedef struct {
	long error;
	long value;
} sbiret;

//* macro per la page table

#define SATP_SV32 (1u << 31)
#define PAGE_V (1 << 0) // bit di validità
#define PAGE_R (1 << 1) // readble
#define PAGE_W (1 << 2) // writable
#define PAGE_X (1 << 3) // Executable
#define PAGE_U (1 << 4) // user (accessibile in user mode)

