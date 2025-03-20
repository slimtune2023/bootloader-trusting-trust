#include "rpi.h"
#include "boot-crc32.h"
#include "boot-defs.h"
#include "cycle-count.h"


uint32_t uart_get32() {
    uint32_t x = uart_get8();
    x |= uart_get8() << 8;
    x |= uart_get8() << 16;
    x |= uart_get8() << 24;
    return x;
}

void uart_put32(uint32_t x) {
    for (int i = 0; i < 4; i++) {
        uart_put8(x & ((1 << 8) - 1));
        x >>= 8;
    }
}

void boot_print(char *msg) {
    uart_put32(PRINT_STRING);
    unsigned n = strlen(msg);
    uart_put32(n);

    for (int i = 0; i < n; i++) {
        uart_put8(msg[i]);
    }
}

uint32_t get_code() {
    uart_init();
    cycle_cnt_init();

    // send every 300ms
    unsigned cyc_300 = CYC_PER_USEC * 300 * 1000;
    unsigned curtime = cycle_cnt_read();

    // gpio_set_output(20);
    // gpio_set_on(20);
    while (1) {
        if (uart_has_data()) {
            // gpio_set_off(20);
            break;
        }
        
        if (cycle_cnt_read() - curtime >= cyc_300) {
            uart_put32(GET_PROG_INFO);
            curtime = cycle_cnt_read();
        }
    }

    uint32_t recieve = uart_get32();
    uint32_t armbase = uart_get32();
    uint32_t n = uart_get32();
    uint32_t crc = uart_get32();

    // if (recieve != PUT_PROG_INFO) {
    //     return 0;
    // }

    // if (n >= 0x200000 - 0x8000) {
    //     return 0;
    // }

    char *msg1 = "James! About to send GET_CODE";
    boot_print(msg1);

    uart_put32(GET_CODE);
    uart_put32(crc);
    
    recieve = uart_get32();
    if (recieve != PUT_CODE) {
        return 0;
    }

    uint8_t *code = (uint8_t *)armbase;

    for (unsigned i = 0; i < n; i++) {
        *(code + i) = uart_get8();
    }

    uint32_t my_crc = crc32(code, n);

    if(my_crc != crc) {
        uart_put32(BOOT_ERROR);
        rpi_reboot();
    }


    char *msg2 = "James! About to send BOOT_SUCCESS";
    boot_print(msg2);
    
    uart_put32(BOOT_SUCCESS);
    uart_flush_tx();

    return armbase;
}