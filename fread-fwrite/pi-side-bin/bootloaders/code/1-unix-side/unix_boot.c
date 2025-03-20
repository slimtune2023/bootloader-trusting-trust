#include "libunix.h"
#include "boot-defs.h"
#include "boot-crc32.h"
#include "unix_boot.h"


int trace_p = 0;

uint32_t get_uint32_check_print(int fd) {
    uint32_t x = get_uint32(fd);
    if (x != PRINT_STRING) {
        return x;
    }

    output("PRINT_STRING:pi sent print: <");
    unsigned n = get_uint32(fd);
    for (int i = 0; i < n; i++) {
        output("%c", get_uint8(fd));
    }
    output(">\n");
    return REDO;
}

uint32_t get_uint32_trace(int fd) {
    uint32_t x = REDO;
    while (x == REDO) {
        x = get_uint32_check_print(fd);
    }

    if (x && trace_p)
        trace("GET32:%x [%s]\n", x, boot_op_to_str(x));
    return x;
}

void put_uint32_trace(int fd, uint32_t u) {
    put_uint32(fd, u);
    if (trace_p)
        trace("PUT32:%x [%s]\n", u, boot_op_to_str(u));
}

uint32_t get_uint8_trace(int fd) {
    uint32_t x = get_uint8(fd);
    if (x && trace_p == 2)
        trace("GET8:%x\n", x);
    return x;
}

void put_uint8_trace(int fd, uint32_t u) {
    put_uint8(fd, u);
    if (trace_p == 2)
        trace("PUT8:%x\n", u);
}

void simple_boot(int fd, uint32_t boot_addr, const uint8_t *buf, unsigned n) {
    uint32_t crc = crc32(buf,n);
    if (trace_p == 1 || trace_p == 2)
        trace("simple_boot: sending %d bytes, crc32=%x\n", n, crc);
    
    uint32_t recieve; // often is the first thing
    while((recieve = get_uint32_trace(fd)) != GET_PROG_INFO) {
        get_uint8_trace(fd);
    }

    put_uint32_trace(fd, PUT_PROG_INFO);
    put_uint32_trace(fd, ARMBASE);
    put_uint32_trace(fd, n);
    put_uint32_trace(fd, crc);

    while (recieve == GET_PROG_INFO) {
        recieve = get_uint32_trace(fd);
    }

    if (recieve != GET_CODE) {
        panic("incorrect get code: expected %x, got %x\n", GET_CODE, recieve);
    }

    recieve = get_uint32_trace(fd);
    if (recieve != crc) {
        panic("incorrect checksum: expected %x, got %x\n", crc, recieve);
    }

    put_uint32_trace(fd, PUT_CODE);
    for (int i = 0; i < n; i++) {
        put_uint8_trace(fd, buf[i]);
    }

    recieve = get_uint32_trace(fd);
    if (recieve == BOOT_ERROR) {
        printf("boot error\n");
    } else if (recieve != BOOT_SUCCESS) {
        printf("didn't recieve boot success or boot error\n");
    }
}
