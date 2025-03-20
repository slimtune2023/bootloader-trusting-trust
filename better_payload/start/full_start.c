#include "rpi.h"
#include "pinned-vm.h"
#include "full-except.h"
#include "memmap.h"

#define MB(x) ((x)*1024*1024)

#define RESERVED 0x6000000

enum { dom_kern = 1 };

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
    // uint32_t BASE = 0x8000;
    // uint32_t offset = 0x4000;
    // uint32_t code = BASE + offset;
    // uint32_t target = 0x6800000;
    // printk("code loc: %x, target: %x\n", code, target);
    // printk("nbytes: %x\n", *(uint32_t *)(code + 12));

    // memcpy((uint32_t *)target, (uint32_t *)code, *(uint32_t *)(code + 12));
    
    // printk("Copied to to: %x, command: %x\n", target + 0x10, *(uint32_t *)(target + 0x10));
    // printk("Abort handler -- %x, command: %x\n", 0x680014c, *(uint32_t *)(0x680014c));
    // BRANCHTO(target + 0x10);
    
    kmalloc_init_set_start((void*)SEG_HEAP, MB(1));
    
    assert((uint32_t)__prog_end__ < SEG_CODE + MB(1));
    assert((uint32_t)__code_start__ >= SEG_CODE);

    // printk("full install start\n");

    // BRANCHTO(0x680027c);

    full_except_install(0);
    // full_except_set_data_abort((full_except_t)(0x680014c));
    full_except_set_data_abort((void (*)(regs_t *))(0x680014c));
    full_except_set_data_abort(data_abort_handler);
    // printk("full install end\n");

    void *null_pt = kmalloc_aligned(4096*4, 1<<14);
    assert((uint32_t)null_pt % (1<<14) == 0);

    assert(!mmu_is_enabled());

    enum { no_user = perm_rw_priv };
    
    pin_t dev  = pin_mk_global(dom_kern, no_user, MEM_device);
    
    pin_t kern = pin_mk_global(dom_kern, no_user, MEM_uncached);
   
    pin_t no_access = pin_mk_global(dom_kern, perm_na_priv, MEM_device);

    mmu_init();

    pin_mmu_sec(0, SEG_CODE, SEG_CODE, kern);
    pin_mmu_sec(1, SEG_HEAP, SEG_HEAP, kern); 

    pin_mmu_sec(2, SEG_STACK, SEG_STACK, kern); 
    pin_mmu_sec(3, SEG_INT_STACK, SEG_INT_STACK, kern); 

    pin_mmu_sec(4, SEG_BCM_0, SEG_BCM_0, dev); 
    pin_mmu_sec(5, SEG_BCM_1, SEG_BCM_1, dev); 

    domain_access_ctrl_set(DOM_client << dom_kern*2); 
    
    enum { ASID = 1, PID = 128 };
    mmu_set_ctx(PID, ASID, null_pt);
    
    // no uart
    pin_mmu_sec(6, SEG_BCM_2, SEG_BCM_2, no_access); 

    // reset reserved section
    *(uint32_t *)RESERVED = 0;


    printk("about to enable\n");

    uint32_t loc = 0x6800000;

    pin_mmu_sec(7, loc, loc, kern); 


    // *(uint32_t *)(loc) = 0xe12fff1e;

    printk("off at %x: %x\n", loc, *(uint32_t *)loc);

    mmu_enable();

    printk("test\n");

    *(uint32_t *)(loc) = 0xe12fff1e;
    printk("on at %x: %x\n", loc, *(uint32_t *)loc);

    mmu_disable();

    printk("off at %x: %x\n", loc, *(uint32_t *)loc);

    mmu_enable();
    
    // BRANCHTO(0x6800000);

    printk("back\n");
}