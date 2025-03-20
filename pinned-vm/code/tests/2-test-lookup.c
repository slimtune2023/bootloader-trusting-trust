/* Miles' little test for tlb_contains_va */

#include "rpi.h"
#include "procmap.h"

void notmain(void) { 
    kmalloc_init_set_start((void*)SEG_HEAP, MB(1));

    // turn on the pinned MMU: identity map.
    procmap_t p = procmap_default_mk(dom_kern);
    procmap_pin_on(&p);

    // if we got here MMU must be working b/c we have accessed GPIO/UART
    trace("hello: mmu must be on\n");

    lockdown_print_entries("before lookup");
    uint32_t result = 0;
    for(int i = 0; i < p.n; i++) {
        uint32_t addr = p.map[i].addr + 0xFAA0;
        if(!tlb_contains_va(&result, addr))
            panic("did not contain valid VA for %x\n", addr);

        if(!tlb_contains_va(&result, addr))
            panic("did not contain valid VA %x\n", addr);
        if(result != addr)
            panic("VA address didn't match!  got=%x, expected=%x", 
                result,addr);
        trace("%x mapped to %x correctly\n", addr, result);
    }
    if(tlb_contains_va(&result, 0xDEADFAA0))
        panic("contained invalid VA\n");

    trace("SUCCESS!\n");
}
