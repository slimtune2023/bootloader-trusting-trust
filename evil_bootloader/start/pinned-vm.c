#include "rpi.h"
#include "libc/bit-support.h"

// has useful enums and helpers.
#include "vector-base.h"
#include "pinned-vm.h"
#include "mmu.h"
#include "procmap.h"

// generate the _get and _set methods.
// (see asm-helpers.h for the cp_asm macro 
// definition)
// arm1176.pdf: 3-149

// static void *null_pt = 0;

void pin_mmu_enable(void) {
    mmu_enable();
}

void pin_mmu_disable(void) {
    mmu_disable();
}

void pin_set_context(uint32_t asid) {
    void *null_pt = kmalloc_aligned(4096*4, 1<<14);
    mmu_set_ctx(128, asid, null_pt);
}

// our main routine: 
//  insert map <va> ==> <pa> with attributes <attr> 
//  at position <idx> in the TLB.
//
// errors:
//  - idx >= 8.
//  - va already mapped.
void pin_mmu_sec(unsigned idx, uint32_t va, uint32_t pa, pin_t attr) {
    assert(idx < 8);
    uint32_t *temp = 0;
    // assert(!tlb_contains_va(temp, va));

    cp15_tlb_idx_set(idx);


    // printk("asid: %x\n", attr.asid);
    unsigned va_set = attr.asid;
    va_set |= attr.G << 9;
    va_set |= va;
    cp15_tlb_va_set(va_set);

    unsigned attr_set = attr.mem_attr << 1;
    attr_set |= attr.dom << 7;
    cp15_tlb_attr_set(attr_set);

    unsigned pa_set = 1;
    pa_set |= attr.AP_perm << 1;
    pa_set |= attr.pagesize << 6; // size; change if not 1mb
    pa_set |= 0b00 << 8;
    pa_set |= pa;
    cp15_tlb_pa_set(pa_set);

    asm volatile ("mcr p15, 0, r0, c7, c10, 4");  // DSB
    asm volatile ("mcr p15, 0, r0, c7, c5, 4");   // prefetch_flush

    // assert(tlb_contains_va(temp, va));
    // branch thingy?
}

// do a manual translation in tlb and see if exists (1)
// returns the result in <result>
//
// low 1..6 bits of result should have the reason.
int tlb_contains_va(uint32_t *result, uint32_t va) {
    cp15_va_trans_set(va);
    *result = cp15_pa_trans_get();
    uint32_t ret = 1-(*result & 1);
    *result &= ~0x3ff;
    *result |= (va & 0x3ff);
    return ret;
}

int pin_exists(uint32_t va, int verbose_p) {
    uint32_t res;
    return tlb_contains_va(&res, va);
}

uint32_t* get_regs_idx(unsigned idx) {
    cp15_tlb_idx_set(idx);

    unsigned va = cp15_tlb_va_get();
    unsigned pa = cp15_tlb_pa_get();
    unsigned attr = cp15_tlb_attr_get();

    asm volatile ("mcr p15, 0, r0, c7, c10, 4");  // DSB
    asm volatile ("mcr p15, 0, r0, c7, c5, 4");   // prefetch_flush

    uint32_t* ret = 0;
    *ret = va;
    *(ret+1) = pa;
    *(ret+2) = attr;
    return ret;
}

// void lockdown_print_entries(const char* msg) {
//     printk("%s\n", msg);
//     unsigned* ret;
//     uint32_t* res = 0;
//     for (int i = 0; i < 8; i++) {
//         ret = get_regs_idx(i);
//         printk("TLB entry %d:\n", i);
//         printk("va: %x\t", ret[0]);
//         printk("pa: %x\t", ret[1]);
//         printk("attr: %x\t", ret[2]);
//         tlb_contains_va(res, ret[0]);
//         printk("contains: %x\n", *res);
//     }
// }

void lockdown_print_entry(unsigned idx) {
    trace("   idx=%d\n", idx);
    // lockdown_index_set(idx);
    // uint32_t va_ent = lockdown_va_get();
    // uint32_t pa_ent = lockdown_pa_get();

    uint32_t *ret = get_regs_idx(idx);
    uint32_t va_ent = ret[0];
    uint32_t pa_ent = ret[1];
    uint32_t attr = ret[2];

    uint32_t v = pa_ent & 0x1; 

    if(!v) {
        trace("     [invalid entry %d]\n", idx);
        return;
    }

    // 3-149
    uint32_t va = (va_ent >> 12);
    uint32_t G = (va_ent >> 9) & 1;
    uint32_t asid = va_ent & 0xff;
    trace("     va_ent=%x: va=%x|G=%d|ASID=%d\n",
        va_ent, va, G, asid);

    // 3-150
    uint32_t pa = (pa_ent >> 12);
    uint32_t nsa = (pa_ent >> 9) & 1;
    uint32_t nstid = (pa_ent >> 8) & 1;
    uint32_t size = (pa_ent >> 6) & 0x3; 
    uint32_t apx = (pa_ent >> 1) & 0x3;
    trace("     pa_ent=%x: pa=%x|nsa=%d|nstid=%d|size=%b|apx=%b|v=%d\n",
                pa_ent, pa, nsa,nstid,size, apx,v);

    // 3-151
    uint32_t dom = (attr >> 7) & 0xf;
    uint32_t xn = (attr >> 6) & 1;
    uint32_t tex = (attr >> 3) & 0x7; 
    uint32_t C = (attr >> 2) & 0x1;
    uint32_t B = (attr >> 1) & 0x1;  
    trace("     attr=%x: dom=%d|xn=%d|tex=%b|C=%d|B=%d\n",
            attr, dom,xn,tex,C,B);
}

void lockdown_print_entries(const char *msg) {
    trace("-----  <%s> ----- \n", msg);
    trace("  pinned TLB lockdown entries:\n");
    for(int i = 0; i < 8; i++)
        lockdown_print_entry(i);
    trace("----- ---------------------------------- \n");
}

void pin_clear(unsigned idx) {
    return;
}

void pin_mmu_init(uint32_t domain_reg) {
    mmu_init();
    domain_access_ctrl_set(domain_reg);
}

