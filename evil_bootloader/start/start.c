#include "rpi.h"
#include "pinned-vm.h"
#include "full-except.h"
#include "memmap.h"

#define MB(x) ((x)*1024*1024)

#define RESERVED 0x6000000

enum { dom_kern = 1 };

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

void notmain(void) {
    uart_init();

    uint32_t BASE = 0x8000;
    uint32_t offset = 0x4000;
    uint32_t code = BASE + offset;
    uint32_t target = 0x6800000;
    printk("code loc: %x, target: %x\n", code, target);
    printk("nbytes: %x\n", *(uint32_t *)(code + 12));

    memcpy((uint32_t *)target, (uint32_t *)code, *(uint32_t *)(code + 12));
    
    printk("About to branch!\n");
    printk("Copied to to: %x, command: %x\n", target + 0x10, *(uint32_t *)(target + 0x10));

    // BRANCHTO(target + 0x10);
    
    kmalloc_init_set_start((void*)SEG_HEAP, MB(1));
    
    assert((uint32_t)__prog_end__ < SEG_CODE + MB(1));
    assert((uint32_t)__code_start__ >= SEG_CODE);

    full_except_install(0);
    full_except_set_data_abort((full_except_t)(0x680014c));
    
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

    mmu_enable();

    BRANCHTO(0x200000);
}