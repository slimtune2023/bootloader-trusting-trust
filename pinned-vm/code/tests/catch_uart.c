#include "rpi.h"
#include "pinned-vm.h"
#include "full-except.h"
#include "memmap.h"

#define MB(x) ((x)*1024*1024)

#define RESERVED 0x6000000

// armv6 has 16 different domains with their own privileges.
// just pick one for the kernel.
enum { dom_kern = 1 };

// reboot
enum { 
    REBOOT_BASE = 0x20100000,
    REBOOT = 0x20100024,
};

// UART
enum {
    AUX_BASE = 0x20210000,
    AUX_ENB = AUX_BASE + 0x5004,
    AUX_IO = AUX_BASE + 0x5040,
    AUX_IER = AUX_BASE + 0x5044,
    AUX_IIR = AUX_BASE + 0x5048,
    AUX_LCR = AUX_BASE + 0x504C,
    AUX_MCR = AUX_BASE + 0x5050,
    AUX_CNTL = AUX_BASE + 0x5060,
    AUX_STAT = AUX_BASE + 0x5064,
    AUX_BAUD = AUX_BASE + 0x5068,
};

enum {
    // code starts at 0x8000, so map the first MB
    //
    // if you look in <libpi/memmap> you can see
    // that all the data is there as well, and we have
    // small binaries, so this will cover data as well.
    //
    // NOTE: obviously it would be better to not have 0 (null) 
    // mapped, but our code starts at 0x8000 and we are using
    // 1mb sections (which require 1MB alignment) so we don't
    // have a choice unless we do some modifications.  
    //
    // you can fix this problem as an extension: very useful!
    SEG_CODE = MB(0),

    // as with previous labs, we initialize 
    // our kernel heap to start at the first 
    // MB. it's 1MB, so fits in a segment. 
    SEG_HEAP = MB(1),

    // if you look in <staff-start.S>, our default
    // stack is at STACK_ADDR, so subtract 1MB to get
    // the stack start.
    SEG_STACK = STACK_ADDR - MB(1),

    // the interrupt stack that we've used all class.
    // (e.g., you can see it in the <full-except-asm.S>)
    // subtract 1MB to get its start
    SEG_INT_STACK = INT_STACK_ADDR - MB(1),

    // the base of the BCM device memory (for GPIO
    // UART, etc).  Three contiguous MB cover it.
    SEG_BCM_0 = 0x20000000,
    SEG_BCM_1 = SEG_BCM_0 + MB(1),
    SEG_BCM_2 = SEG_BCM_0 + MB(2),

    // we guarantee this (2MB) is an 
    // unmapped address
    SEG_ILLEGAL = MB(2),
};

#include "asm-helpers.h"

cp_asm_get(cp15_fault_addr, p15, 0, c6, c0, 0);
cp_asm_get(cp15_dfsr, p15, 0, c5, c0, 0);
cp_asm_get(cp15_ifsr, p15, 0, c5, c0, 1);

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

    enum { no_user = perm_rw_priv };

    // pin_t dev  = pin_mk_global(dom_kern, no_user, MEM_device);
    // pin_mmu_sec(4, SEG_BCM_0, SEG_BCM_0, dev); 

    if (fault_addr == AUX_STAT && !write) {
        // printk("waiting for uart to free\n");
        do {
            r->regs[0] = GET32(AUX_STAT);
        } while (!(GET32(AUX_STAT) & 2));

        // printk("AUX_STAT: %x\n", r->regs[0]);
    }

    if (fault_addr == AUX_IO && write) {
        // printk("TRIED TO WRITE: %c\n", r->regs[1]);
        printk("%c", (char)r->regs[1]);
        uint32_t ct = *(uint32_t *)RESERVED;
        *(char *)(RESERVED + 4 + ct) = (char)r->regs[1];
        (*(uint32_t *)RESERVED) ++;
    }

    // domain_access_ctrl_set(DOM_client << (dom_kern * 2));

    mmu_enable();

    switchto(r);
}

// static void prefetch_abort_handler(regs_t *r) {
//     printk("prefetch abort\n");
//     uint32_t fault_addr = cp15_fault_addr_get();
//     uint32_t ifsr = cp15_ifsr_get();

//     uint32_t pc = r->regs[15];
    
//     enum { no_user = perm_rw_priv };

//     mmu_disable();
//     pin_t kern = pin_mk_global(dom_kern, no_user, MEM_uncached);
//     pin_mmu_sec(1, SEG_HEAP, SEG_HEAP, kern);
//     mmu_enable();

//     printk("PREFETCH FAULT: PC = %x, Fault Address = %x, IFSR = %x\n", pc, fault_addr, ifsr);

//     switchto(r);
//     // domain_access_ctrl_set(DOM_client << (dom_kern * 2));
// }

void notmain(void) {
    // ******************************************************
    // 1. one-time initialization.
    //   - create an empty page table (to catch errors).
    //   - setup exception handlers.
    //   - compute permissions for no-user-access.

    // initialize the heap.  as stated above,
    // starts at 1MB, and is 1MB big.
    kmalloc_init_set_start((void*)SEG_HEAP, MB(1));

    // example sanity check that entire process fits 
    // within 1MB segment: these symbols are in <memmap.h> 
    // and defined in <libpi/memmap> (we've used in previous
    // labs).
    assert((uint32_t)__prog_end__ < SEG_CODE + MB(1));
    assert((uint32_t)__code_start__ >= SEG_CODE);

    // setup a data abort handler (just like last lab).
    full_except_install(0);
    full_except_set_data_abort(data_abort_handler);
    // full_except_set_prefetch(prefetch_abort_handler);

    // allocate a page table with invalid
    // entries.
    //
    // we will TLB pin all valid mappings.  
    // if the code is correct, the hardware
    // will never look anything up in the page
    // table.
    // 
    // however, if the code is buggy and does a 
    // a wild memory access that isn't in any
    // pinnned entry, the hardware would then try 
    // to look the address up in the page table
    // pointed to by the tlbwr0 register.
    //
    // if this page table isn't explicitly
    // initialized to invalid entries, the hardware
    // would interpret the garbage bits there 
    // valid and potentially insert them (very
    // hard bug to find).
    //
    // to prevent this we allocate a zero-filled 
    // page table.
    //  - 4GB/1MB section * 4 bytes = 4096*4.
    //  - zero-initialized will set each entry's 
    //    valid bit to 0 (invalid).
    //  - lower 14 bits (16k aligned) as required
    //    by the hardware.
    void *null_pt = kmalloc_aligned(4096*4, 1<<14);
    assert((uint32_t)null_pt % (1<<14) == 0);

    // in <mmu.h>: checks cp15 control reg 1.
    //  - will implement in next vm lab.
    assert(!mmu_is_enabled());

    // no access for user (r/w privileged only)
    // defined in <mem-attr.h>.  
    //
    // is APX and AP fields bit-wise or'd
    // together: (APX << 2 | AP) see 
    //  - 3-151 for table, or B4-9
    enum { no_user = perm_rw_priv };

    // attribute for device memory (see <pin.h>).  this 
    // is needed when pinning device memory:
    //   - permissions: kernel domain, no user access, 
    //   - memory rules: strongly ordered, not shared.
    //     see <mem-attr.h> for <MEM_device>
    pin_t dev  = pin_mk_global(dom_kern, no_user, MEM_device);

    // attribute for kernel memory (see <pin.h>)
    //   - protection: same as device; we don't want the
    //     user to read/write it.
    //   - memory rules: uncached access.  you can start
    //     messing with this for speed, though have to 
    //     do cache coherency maintance
    pin_t kern = pin_mk_global(dom_kern, no_user, MEM_uncached);
   
    pin_t no_access = pin_mk_global(dom_kern, perm_na_priv, MEM_device);

    mmu_init();

    // printk("Before first iter:\n");
    // lockdown_print_entries("");


    pin_mmu_sec(0, SEG_CODE, SEG_CODE, kern);
    pin_mmu_sec(1, SEG_HEAP, SEG_HEAP, kern); 

    // Q2: if you comment this out, what happens?
    pin_mmu_sec(2, SEG_STACK, SEG_STACK, kern); 
    // Q3: if you comment this out, what happens?
    pin_mmu_sec(3, SEG_INT_STACK, SEG_INT_STACK, kern); 

    // map all device memory to itself.  ("identity map")
    // pin_mmu_sec(4, SEG_BCM_0, SEG_BCM_0, dev); 
    // pin_mmu_sec(5, SEG_BCM_1, SEG_BCM_1, dev); 
    // pin_mmu_sec(6, SEG_BCM_2, SEG_BCM_2, dev); 

    // printk("At end:\n");
    // lockdown_print_entries("");

    domain_access_ctrl_set(DOM_client << dom_kern*2); 
    // domain_access_ctrl_set(~0); 

    // set address space id, page table, and pid.
    // note:
    //  - <pid> is ignored by the hw: it's just to help the os.
    //  - <asid> doesn't matter for this test b/c all entries 
    //    are global.
    //  - recall the page table has all entries invalid and is
    //    just to catch memory errors.
    enum { ASID = 1, PID = 128 };
    mmu_set_ctx(PID, ASID, null_pt);

    // if you want to see the lockdown entries.
    // lockdown_print_entries("about to turn on first time");

    // ******************************************************
    // 4. turn MMU on/off, checking that it worked.
    // trace("about to enable\n");
    // mmu_enable();

    // assert(mmu_is_enabled());

    // printk("MMU enabled\n");

    // mmu_disable();

    printk("Testing printk with uart data aborts\n");
    
    // no uart
    pin_mmu_sec(4, SEG_BCM_2, SEG_BCM_2, no_access); 
    // no reboot
    pin_mmu_sec(5, REBOOT_BASE, REBOOT_BASE, no_access); 

    // reset reserved section
    *(uint32_t *)RESERVED = 0;

    mmu_enable();

    printk("test\n");

    mmu_disable();

    uint32_t nbytes = *(uint32_t *)RESERVED;
    char *buf = (char *)(RESERVED + 4);
    printk("\n-----------------\nRECIEVED MESSAGE (nbytes = %x): \n", nbytes);
    for (int i = 0; i < nbytes; i++) {
        printk("%c", buf[i]);
    }\
    printk("\n-----------------\n\n");
    printk("SUCCESS!!\n");
}
