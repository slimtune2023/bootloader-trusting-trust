@ Must carefully read B2 of the armv6 manual.  You cannot rely on "well it
@ worked on my test case": mistakes cause stale data, which may or may not
@ show up in your test case (likely not) despite being wildly broken.
@
@ Note: Rd is allegedly a read-only parameter to these instructions, but
@ the manual says SBZ (should be zero).  I think (and the linux source seems
@ to think) that this means we have to explicitly set it to 0.  Not setting
@ Rd=0 is an easy mistake to make.  Perhaps is worth building in (Linux does
@ not, but) --- the cost of an extra mov is negligible given our other 
@ overheads.
@
@ Alot of people put these instructions in inline assembly, wrapped up by 
@ function calls (e.g., cs107e's MMU code).  This practice is dangerous. 
@ For example, if you are flushing caches, you cannot be sure that the 
@ function return, etc does not then reload them, esp w.r.t. the BTB.  I 
@ think the only safe approach is to put directly in assembly so you are 
@ guaranteed no branch, indirect jump, load/store happens in between.
@
@ A lot of MMU code out there appears pretty broken b/c they don't do enough
@ flushing operations and/or rely on gcc code generation to not do the above.

#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ many page numbers are in 
@    armv6-coprocessor-asm.h
@ and the other armv6-*.h files

// just read the cache type: use C code to figure out if it is unified
// b6-13, b6-14
MK_FN(get_cache_type)
    asm_not_implemented()
    bx lr

// void reset(void)
// clear the dcache, icache, tlbs
MK_FN(mmu_reset)
    @ b staff_mmu_reset

    @ asm_not_implemented()
    mov r2, #0

    // cache invalidation
    INV_ALL_CACHES(r2)

    // tlb invalidation
    INV_TLB(r2)

    // make visible (procedure from B2-23)
    DSB(r2)

    // not sure if this is necessary, but included because of B2-23
    FLUSH_BTB(r2)

    // for flush_btb
    PREFETCH_FLUSH(r2)
    bx lr


// ---------------
// DONE IN MMU.H
// ---------------
// void cp15_domain_ctrl_wr(uint32_t dom_reg);
//  need to do cp15 sync: flush_btb, dsb, prefetch flush
MK_FN(cp15_domain_ctrl_wr)
    @ b staff_cp15_domain_ctrl_wr

    mcr p15, #0, r0, c3, c0, #0
    mov r2, #0
    PREFETCH_FLUSH(r2)
    bx lr

// void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);
// sequence from b2-25
MK_FN(cp15_set_procid_ttbr0)
    @ b staff_cp15_set_procid_ttbr0

    @ asm_not_implemented()
    
    mov r2, #0

    // must do before asid to have effect
    DSB(r2)

    // asid to 0, pf on cp15 reg change
    ASID_SET(r2)
    PREFETCH_FLUSH(r2)

    // ttbr0 to pt, ttbr1 and ttbc to 0
    TTBR0_SET(r1)
    TTBR1_SET(r2)
    TTBR_BASE_CTRL_WR(r2)

    // btb flush on page table change w/ pf
    FLUSH_BTB(r2)
    PREFETCH_FLUSH(r2)

    // set asid to desired value, then btb flush + pf
    ASID_SET(r0)
    FLUSH_BTB(r2)
    PREFETCH_FLUSH(r2)

    bx lr

// void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
MK_FN(mmu_disable_set_asm)
    @ b staff_mmu_disable_set_asm

    @ note: this piece of code is mapped into the same place in both
    @ MMU-enabled and MMU-disabled so BTB and Prefetch entries work identically
    @ to both MMU-enable and disable.   Without this requirement I believe is
    @ very hard to get right (perhaps impossible?) since we cannot flush
    @ the BTB and prefetch and switch MMU on/off atomically.  thus, would
    @ flush, but instructions could get fetched before we could disable,
    @ defeating the purpose.

    @ asm_not_implemented()

    mov r2, #0
    // B4-7
    CLEAN_INV_DCACHE(r2)
    // after inv dcache
    DSB(r2)

    CONTROL_REG1_WR(r2)
    // coproc write
    PREFETCH_FLUSH(r2)

    // For tests
    INV_ICACHE(r2)
    // after inv icache
    DSB(r2)

    // b2-24
    FLUSH_BTB(r2)
    // after btb flush
    PREFETCH_FLUSH(r2)


    bx lr

// void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
MK_FN(mmu_enable_set_asm)
    @ b staff_mmu_enable_set_asm

    @ asm_not_implemented()

    mov r1, r0
    mov r2, #0

    // turn off icache    
    CONTROL_REG1_RD(r0)
    bic r0, r0, #0x1000
    CONTROL_REG1_WR(r0)
    // after cp change
    PREFETCH_FLUSH(r2)

    // invalidate icache
    INV_ICACHE(r2)
    // icache op visible
    DSB(r2)

    // set reg1 with actual value
    mov r0, r1
    CONTROL_REG1_WR(r0)
    PREFETCH_FLUSH(r2)

    // flush btb
    FLUSH_BTB(r2)
    // for btb visible
    PREFETCH_FLUSH(r2)

    bx lr

MK_FN(mmu_sync_pte_mods)
    @ b staff_mmu_sync_pte_mods

    @ asm_not_implemented()

    mov r2, #0

    // Clear and inv dcache, inv icache (b2-23)
    CLEAN_INV_DCACHE(r2)
    INV_ICACHE(r2)
    // after inv caches
    DSB(r2)

    // b2-23
    INV_TLB(r2)
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)

    bx lr

MK_FN(cp15_ctrl_reg1_get)
    mrc	p15, #0x0, r0, c1, c0, #0x0
    bx lr

MK_FN(cp15_ctrl_reg1_wr)
    mov	r2, #0
    mcr	p15, #0x0, r0, c1, c0, #0x0
    DSB(r2);
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)
    bx	lr