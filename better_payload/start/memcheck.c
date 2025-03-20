#include "rpi.h"

void notmain(void) {
    printk("Starting!\n");
    uint32_t loc = 0x6000000;
    printk("val at %x: %x\n", loc, *(uint32_t *)loc);
}