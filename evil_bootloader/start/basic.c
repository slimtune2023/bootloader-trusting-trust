#include "rpi.h"

// #include "cycle-count.h"

// #define PRINT_STRING 0xDDDDEEEE

// void uart_put32(uint32_t x) {
//     for (int i = 0; i < 4; i++) {
//         uart_put8(x & ((1 << 8) - 1));
//         x >>= 8;
//     }
// }

// void boot_print(char *msg) {
//     uart_put32(PRINT_STRING);
//     uart_put32(PRINT_STRING);
//     uart_put32(PRINT_STRING);
//     uart_put32(PRINT_STRING);
//     uart_put32(PRINT_STRING);
//     unsigned n = strlen(msg);
//     uart_put32(n);

//     for (int i = 0; i < n; i++) {
//         uart_put8(msg[i]);
//     }
// }

void notmain(void) {

    // uart_init();
    // cycle_cnt_init();

    // boot_print("test!");
    // uart_flush_tx();
    // for (long i = 0; i < 100000000000; i++) {
    //     asm volatile("nop");
    // }

    *(uint32_t *)(0x6000000) = 0xdeadbeef;

    BRANCHTO(0x200000);
}