#include "rpi.h"
#include "pinned-vm.h"
#include "full-except.h"
#include "memmap.h"

void notmain(void) {
    for (char c = 'a'; c <= 'z'; c++) {
        uart_put8(c);
    }
    uart_put8('\n');
}