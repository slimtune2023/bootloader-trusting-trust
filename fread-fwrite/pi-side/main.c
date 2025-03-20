#include "rpi.h"
#include "cycle-count.h"

#include "boot-defs.h"
#include "boot-crc32.h"

void uart_put32(unsigned val) {
    uart_put8(val & 0xff);
    uart_put8((val >> 8) & 0xff);
    uart_put8((val >> 16) & 0xff);
    uart_put8((val >> 24) & 0xff);
}

unsigned uart_get32() {
    unsigned val = 0;
    val |= uart_get8();
    val |= (uart_get8() << 8);
    val |= (uart_get8() << 16);
    val |= (uart_get8() << 24);

    return val;
}

void boot_putk(char *s) {
    // get length of string
    unsigned n = 0; // strlen(s);
    for (; *(s+n); n++);
    // while(*(s+(n++)));

    // put PRINT_STRING code
    uart_put32(PRINT_STRING);
    uart_put32(n);
    for (unsigned i=0; i < n; i++) {
        uart_put8(*(s+i));
    }
}

void _cstart() {
    // initialize stuff
    uart_init();
    cycle_cnt_init();

    // send GET_PROG_INFO code every 300 ms
    unsigned cyc300 = CYC_PER_USEC * 300 * 1000;
    unsigned base_cnt = cycle_cnt_read();

    while (1) {
        if (uart_has_data()) {
            break;
        } else {
            unsigned cyc_cnt = cycle_cnt_read();

            if (cyc_cnt - base_cnt > cyc300) {
                // send GET_PROG_INFO code
                uart_put32(GET_PROG_INFO);
                base_cnt = cyc_cnt;
            }
        }
    }

    // receive PUT_PROG_INFO and code-related values
    unsigned val = uart_get32();
    unsigned addr = uart_get32();
    unsigned nbytes = uart_get32();
    unsigned rcv_crc = uart_get32();

    if (val != PUT_PROG_INFO) {
        uart_put32(BOOT_ERROR);
        uart_disable();
        rpi_reboot();
    }

    // check if addr matches ARM_BASE
    if (((unsigned) addr) != ARMBASE) {
        uart_put32(BOOT_ERROR);
        uart_disable();
        rpi_reboot();
    }

    // check if code collision
    if (addr + nbytes >= 0x200000) {
        uart_put32(BOOT_ERROR);
        uart_disable();
        rpi_reboot();
    }

    // add print statement
    boot_putk("PI-SIDE: Sai was here\n");

    // send GET_CODE and received crc value
    uart_put32(GET_CODE);
    uart_put32(rcv_crc);

    // receive PUT_CODE and copy code to addr
    if (uart_get32() != PUT_CODE) {
        uart_put32(BOOT_ERROR);
        uart_disable();
        rpi_reboot();
    }

    uint8_t *start_addr = (uint8_t *) ARMBASE;
    for (unsigned i=0; i < nbytes; i++) {
        *(start_addr + i) = uart_get8();
    }

    // check if crc matches received crc
    unsigned crc = crc32(start_addr, nbytes);    
    if (crc != rcv_crc) {
        uart_put32(BOOT_ERROR);
        uart_disable();
        rpi_reboot();
    }

    // send success code and jump to start of code
    boot_putk("PI-SIDE: Received program!\n");
    uart_put32(BOOT_SUCCESS);
    uart_flush_tx();

    BRANCHTO(ARMBASE);
}