#include "rpi.h"
#include "mmu.h"
#include "full-except.h"
#include "asm-helpers.h"
#include "pi-sd.h"
#include "fat32.h"

#define MB(x) ((x)*1024*1024)

#define RESERVED 0x6000000

enum {
    AUX_BASE = 0x20210000,
    AUX_IO = AUX_BASE + 0x5040,
    AUX_STAT = AUX_BASE + 0x5064,
};

enum {
    SEG_CODE = MB(0),
    SEG_HEAP = MB(1),
    SEG_STACK = STACK_ADDR - MB(1),
    SEG_INT_STACK = INT_STACK_ADDR - MB(1),
    SEG_BCM_0 = 0x20000000,
    SEG_BCM_1 = SEG_BCM_0 + MB(1),
    SEG_BCM_2 = SEG_BCM_0 + MB(2),
    SEG_ILLEGAL = MB(2),
};

cp_asm_get(cp15_fault_addr, p15, 0, c6, c0, 0);
cp_asm_get(cp15_dfsr, p15, 0, c5, c0, 0);

void printfs(void) {
    kmalloc_init_set_start((uint32_t *)0x7000000, FAT32_HEAP_MB);
    pi_sd_init();
  
    // printk("Reading the MBR.\n");
    mbr_t *mbr = mbr_read();
  
    // printk("Loading the first partition.\n");
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    assert(mbr_part_is_fat32(partition.part_type));
  
    // printk("Loading the FAT.\n");
    fat32_fs_t fs = fat32_mk(&partition);
  
    printk("Loading the root directory.\n");
  
    pi_dirent_t root = fat32_get_root(&fs);
  
    printk("Listing files:\n");
    uint32_t n;
    pi_directory_t files = fat32_readdir(&fs, &root);
    printk("Got %d files.\n", files.ndirents);
    for (int i = 0; i < files.ndirents; i++) {
      if (files.dirents[i].is_dir_p) {
        printk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
      } else {
        printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
      }
    }
  
    printk("\nLooking for secret.txt.\n");
    char *name = "SECRET.TXT";
    pi_dirent_t *config = fat32_stat(&fs, &root, name);
    demand(config, secret.txt not found!\n);
  
    printk("Reading secret.txt.\n");
    pi_file_t *file = fat32_read(&fs, &root, name);
  
    printk("Printing secret.txt (%d bytes):\n", file->n_data);
    printk("--------------------\n");
    for (int i = 0; i < file->n_data; i++) {
      printk("%c", file->data[i]);
    }
    printk("--------------------\n");
  }

void cleanup(void) {
    // mmu_disable();
    assert(!mmu_is_enabled());

    printk("-");

    uint32_t nbytes = *(uint32_t *)RESERVED;
    char *buf = (char *)(RESERVED + 4);
    printk("\n-----------------\nRECIEVED MESSAGE (nbytes = %x): \n", nbytes);
    for (int i = 0; i < nbytes; i++) {
        printk("%c", buf[i]);
    }
    
    printfs();

    // this triggers done
    printk("\n-----------------\n\n");
    printk("SUCCESS!!\n");

    uart_flush_tx();
    delay_ms(10);

    // rpi_reboot();
}

// Fault handler to verify exception details
static void data_abort_handler(regs_t *r) {
    mmu_disable();

    // printk("data abort\n");
    uint32_t fault_addr = cp15_fault_addr_get();
    uint32_t dfsr = cp15_dfsr_get();
    uint32_t write = (dfsr >> 11) & 1;

    uint32_t pc = r->regs[15];

    r->regs[15]+=4;

    // printk("FAULT: PC = %x, Fault Address = %x, DFSR = %x, WRITE = %x\n", pc, fault_addr, dfsr, write);

    // enum { no_user = perm_rw_priv };

    if (fault_addr == AUX_STAT && !write) {
        do {
            r->regs[0] = GET32(AUX_STAT);
        } while (!(GET32(AUX_STAT) & 2));
    } else if (fault_addr == AUX_IO && write) {
        printk("%c", (char)r->regs[1]);
        uint32_t ct = *(uint32_t *)RESERVED;
        *(char *)(RESERVED + 4 + ct) = (char)r->regs[1];
        (*(uint32_t *)RESERVED) ++;

        if (ct >= 6) {
            if (strncmp((char *)(RESERVED + 4 + ct - 6), "DONE!!!", 7) == 0) {
                cleanup();
                switchto(r);
            }
        }
    } else if (fault_addr == 0x20215064 && !write) {
        // dev barrier
        dev_barrier();
    } else {
        // printk("FAULT: PC = %x, Fault Address = %x, DFSR = %x, WRITE = %x\n", pc, fault_addr, dfsr, write);
    }

    mmu_enable();

    switchto(r);
}

void notmain(void) {
    printk("In handler!\n");
    full_except_install(0);
    full_except_set_data_abort(data_abort_handler);
}