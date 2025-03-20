#ifndef __SIMPLE_MMU_H__
#define __SIMPLE_MMU_H__
#include "armv6-cp15.h"
#include "asm-helpers.h"

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

#define coproc_mk(fn, coproc, opcode_1, Crn, Crm, opcode_2)     \
coproc_mk_set(fn, coproc, opcode_1, Crn, Crm, opcode_2)        \
coproc_mk_get(fn, coproc, opcode_1, Crn, Crm, opcode_2) 

// low level MMU hardware routines.  agnostic
// as to whether we use a page table or pinning.
//
// workflow:
//  1. init mmu before doing anything.
//  2. then setup:
//   - domain permissions
//   - the asid
//   - page table (invalid or valid)
//   - any pinned entries.
// 3. then can turn the mmu on.
// 4. then can read/write memory.
// 5. then can turn the mmu off.
//
// NOTE: there are generally single hardware instructions
// for each of the above, HOWEVER, just issuing the 
// instruction is rarely enough. there is typically
// a recipe to follow to ensure that hardware state
// is consistent.  See the upcoming vm coherence lab.

// One time hardware initialization.  
// Do before anything else!
void staff_mmu_init(void);
void mmu_init(void);

coproc_mk(dom, p15, 0, c3, c0, 0)

// Set the domain access control to <d>.
// do before turning MMU on!
void staff_domain_access_ctrl_set(uint32_t d);
void domain_access_ctrl_set(uint32_t d);

// get
uint32_t staff_domain_access_ctrl_get(void);
uint32_t domain_access_ctrl_get(void);


// internal routine to set the hardware state: 
//   - <asid> is address space identifier.
//   - <pt> is the 2^14 aligned page table.
//
// do before turning MMU on!
void staff_set_procid_ttbr0(unsigned pid, unsigned asid, void *pt);
void set_procid_ttbr0(unsigned pid, unsigned asid, void *pt);

// called to sync up the hw state after modifying tlb entry
void staff_sync_tlb(void);

// setup pid, asid and pt in hardware.
// must call:
//  1. before turning MMU on at all
//  2. when switching address spaces (or asid won't
//     be correct).
void staff_mmu_set_ctx(uint32_t pid, uint32_t asid, void *pt);
void mmu_set_ctx(uint32_t pid, uint32_t asid, void *pt);


// called to sync after a set of pte modifications: flushes everything.
void mmu_sync_pte_mods(void);
void staff_mmu_sync_pte_mods(void);



#if 0
static inline void 
staff_mmu_set_ctx(uint32_t pid, uint32_t asid, void *pt) {
    assert(asid!=0);
    assert(asid<64);
    staff_set_procid_ttbr0(pid, asid, pt);
}

static inline void 
mmu_set_ctx(uint32_t pid, uint32_t asid, void *pt) {
    assert(asid!=0);
    assert(asid<64);
    set_procid_ttbr0(pid, asid, pt);
}
#endif

// turn the MMU on: 
// you must have previously done:
//  - <mmu_init> or hardware could have garbage.
//  - <mmu_set_ctx>: or there is no asid and page 
//    table for the hw to use.
//  - <domain_access_control_set> or no domain
//    permissions will be setup and you will immediately
//    fault.
void staff_mmu_enable(void);
void mmu_enable(void);

// turn mmu off, write-back data cache if needed, etc.
void staff_mmu_disable(void);
void mmu_disable(void);

// helper routine: is mmu on?
int staff_mmu_is_enabled(void);
int mmu_is_enabled(void);

void reset(void);
void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
void mmu_sync_pte_mods(void);
void cp15_domain_ctrl_wr(uint32_t dom_reg);


#endif
