#ifndef __PINNED_VM_H__
#define __PINNED_VM_H__
// simple interface for pinned virtual memory: most of it is 
// enum and data structures.  you'll build three routines:

#include "mmu.h"
#include "mem-attr.h"
#include "asm-helpers.h"
#include "armv6-coprocessor-asm.h"

// you can flip these back and forth if you want debug output.
#if 0
    // change <output> to <debug> if you want file/line/func
#   define pin_debug(args...) output("PIN_VM:" args)
#else
#   define pin_debug(args...) do { } while(0)
#endif

#define coproc_mk_set(fn_name, coproc, opcode_1, Crn, Crm, opcode_2)       \
    static inline void c ## coproc ## _ ## fn_name ## _set(uint32_t v) {                    \
        asm volatile ("mcr " MK_STR(coproc) ", "                        \
                             MK_STR(opcode_1) ", "                      \
                             "%0, "                                     \
                            MK_STR(Crn) ", "                            \
                            MK_STR(Crm) ", "                            \
                            MK_STR(opcode_2) :: "r" (v));               \
        prefetch_flush();                                               \
    }

#define coproc_mk_get(fn_name, coproc, opcode_1, Crn, Crm, opcode_2)       \
    static inline uint32_t c ## coproc ## _ ## fn_name ## _get(void) {                      \
        uint32_t ret=0;                                                   \
        asm volatile ("mrc " MK_STR(coproc) ", "                        \
                             MK_STR(opcode_1) ", "                      \
                             "%0, "                                     \
                            MK_STR(Crn) ", "                            \
                            MK_STR(Crm) ", "                            \
                            MK_STR(opcode_2) : "=r" (ret));             \
        return ret;                                                     \
    }


// make both get and set methods.
#define coproc_mk(fn, coproc, opcode_1, Crn, Crm, opcode_2)     \
    coproc_mk_set(fn, coproc, opcode_1, Crn, Crm, opcode_2)        \
    coproc_mk_get(fn, coproc, opcode_1, Crn, Crm, opcode_2) 

coproc_mk(tlb_idx, p15, 5, c15, c4, 2)
coproc_mk(tlb_va, p15, 5, c15, c5, 2)
coproc_mk(tlb_pa, p15, 5, c15, c6, 2)
coproc_mk(tlb_attr, p15, 5, c15, c7, 2)
coproc_mk_set(va_trans, p15, 0, c7, c8, 0) // op code 2?
coproc_mk_get(pa_trans, p15, 0, c7, c4, 0)

// attributes: these get inserted into the TLB.
typedef struct {
    // for today we only handle 1MB sections.
    uint32_t G,         // is this a global entry?
             asid,      // don't think this should actually be in this.
             dom,       // domain id
             pagesize;  // can be 1MB or 16MB

    // permissions for needed to access page.
    //
    // see mem_perm_t above: is the bit merge of 
    // APX and AP so can or into AP position.
    mem_perm_t  AP_perm;

    // caching policy for this memory.
    // 
    // see mem_cache_t enum above.
    // see table on 6-16 b4-8/9
    // for today everything is uncacheable.
    mem_attr_t mem_attr;
} pin_t;

// pinned encodings for different page sizes.
enum {
    PAGE_4K     = 0b01,
    PAGE_64K    = 0b10,
    PAGE_1MB    = 0b11,
    PAGE_16MB   = 0b00
};

static inline pin_t pin_16mb(pin_t p) {
    p.pagesize = PAGE_16MB;
    return p;
}

// enable MMU -- must have set the context
// first.
void pin_mmu_enable(void);

// disable MMU
void pin_mmu_disable(void);

// mmu can be off or on: setup address space context.
// must do before enabling MMU or when switching
// between address spaces.
void pin_set_context(uint32_t asid);

// our main routine: 
//  insert map <va> ==> <pa> with attributes <attr> 
//  at position <idx> in the TLB.
//
// errors:
//  - idx >= 8.
//  - va already mapped.
void pin_mmu_sec(unsigned idx,
                uint32_t va,
                uint32_t pa,
                pin_t attr);

// do a manual translation in tlb and see if exists (1)
// returns the result in <result>
//
// low 1..6 bits of result should have the reason.
int tlb_contains_va(uint32_t *result, uint32_t va);

// wrapper that check that <va> is pinned in the tlb
int pin_exists(uint32_t va, int verbose_p);

// print <msg> then all valid pinned entries.
// note: if you print everything you see a lot of
// garbage in the non-initialized entires --- i'm 
// not sure if we are guaranteed that these have their
// valid bit set to 0?   
void lockdown_print_entries(const char* msg);

// set pin index <idx> to all 0s.
void pin_clear(unsigned idx);

void pin_mmu_init(uint32_t domain_reg);

// void staff_pin_mmu_init(uint32_t domain_reg);

static inline pin_t pin_mk_global(uint32_t dom, mem_perm_t perm, mem_attr_t attr) {
    pin_t p;
    p.G = 1;
    p.asid = 0;
    p.dom = dom;
    p.pagesize = 0b11;
    p.AP_perm = perm;
    p.mem_attr = attr;
    return p;
}

static inline pin_t pin_mk_user(uint32_t dom, uint32_t asid, mem_perm_t perm, mem_attr_t attr) {
    pin_t p;
    p.G = 0;
    p.asid = asid;
    p.dom = dom;
    p.pagesize = 0b11;
    p.AP_perm = perm;
    p.mem_attr = attr;
    return p;
}

static inline pin_t pin_mk_device(uint32_t dom) {
    pin_t p;
    p.G = 1;
    p.asid = 0;  // unset
    p.dom = dom;
    p.pagesize = 0b11;
    p.AP_perm = perm_rw_priv;
    p.mem_attr = MEM_device;
    return p;
}

#endif