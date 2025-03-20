#include "rpi.h"
#include "pi_boot.h"

void notmain(void) {
    uint32_t addr = get_code();
    if(!addr)
        rpi_reboot();

    // blx to addr.  
    // could also call it as a function pointer.
    BRANCHTO(addr);
    not_reached();
}
