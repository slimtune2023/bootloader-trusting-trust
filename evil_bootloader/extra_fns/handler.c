#include "rpi.h"
#include "mmu.h"
#include "full-except.h"
#include "asm-helpers.h"

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
    } else {
        printk("FAULT: PC = %x, Fault Address = %x, DFSR = %x, WRITE = %x\n", pc, fault_addr, dfsr, write);
    }

    mmu_enable();

    switchto(r);
}

void notmain(void) {
    full_except_install(0);
    full_except_set_data_abort(data_abort_handler);
}