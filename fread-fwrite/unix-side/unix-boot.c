#include "unix-boot.h"
#include "boot-crc32.h"

void boot_put8(int fd, uint8_t val) {
    if (trace_p == TRACE_ALL) {
        trace("PUT8:%x\n", val);
    }

    int res = write_exact(fd, &val, 1);
}

void boot_put32(int fd, uint32_t val) {
    if (trace_p == TRACE_CONTROL_ONLY || trace_p == TRACE_ALL) {
        trace("PUT32:%x [%s]\n", val, boot_op_to_str(val));
    }

    int res = write_exact(fd, &val, 4);
}

uint8_t boot_get8_raw(int fd) {
    uint8_t byte;
    int res = read_exact(fd, &byte, 1);

    assert(res == 1);
    return byte;
}

uint8_t boot_get8(int fd) {
    uint8_t byte;
    int res = read_exact(fd, &byte, 1);
    
    if (trace_p == TRACE_ALL) {
        trace("GET8:%x\n", byte);
    }

    assert(res == 1);
    return byte;
}

uint32_t boot_get32_raw(int fd) {
    uint32_t val = 0;
    uint8_t byte;

    for (unsigned i=0; i < 4; i++) {
        int res = read_exact(fd, &byte, 1);
        assert(res == 1);
        
        val |= byte << (8 * i);
    }

    return val;
}

uint32_t boot_get32(int fd) {
    uint32_t val = boot_get32_raw(fd);

    while (val == PRINT_STRING) {
        unsigned n = boot_get32_raw(fd);
        for (unsigned i=0; i < n; i++) {
            printf("%c", (char) boot_get8_raw(fd));
        }

        val = boot_get32_raw(fd);
    }

    if (trace_p == TRACE_CONTROL_ONLY || trace_p == TRACE_ALL) {
        trace("GET32:%x [%s]\n", val, boot_op_to_str(val));
    }

    return val;
}

void simple_boot(int fd, unsigned boot_addr, const uint8_t *code, unsigned nbytes) {
    unsigned crc = crc32(code, nbytes);
    trace("simple_boot: sending %d bytes, crc32=%x\n", nbytes, crc);
    boot_output("waiting for a start\n");

    // unix step 1
    uint32_t val;
    while ((val = boot_get32(fd)) != GET_PROG_INFO)
        val = boot_get8(fd);

    boot_put32(fd, PUT_PROG_INFO);
    boot_put32(fd, boot_addr);
    boot_put32(fd, nbytes);    
    boot_put32(fd, crc);

    // unix step 2
    while((val = boot_get32(fd)) != GET_CODE);
    
    if (val == BOOT_ERROR) {
        printf("error on pi side, exiting...\n");
        return;
    } else if (val != GET_CODE) {
        printf("weird output\n");
        return;
    }

    if (boot_get32(fd) != crc) {
        printf("crc sent from pi doesn't match...\n");
        return;
    }

    boot_put32(fd, PUT_CODE);
    for (unsigned i=0; i < nbytes; i++) {
        boot_put8(fd, *(code + i));
    }

    // unix step 3
    val = boot_get32(fd);
    if (val == BOOT_ERROR) {
        printf("error on pi side, exiting...\n");
        return;
    } else if (val != BOOT_SUCCESS) {
        printf("weird output\n");
        return;
    }

    boot_output("bootloader: Done.\n");
    
    return;
}

void send_file(int fd, uint32_t file_addr, const uint8_t *file, unsigned nbytes, char *filename) {
    for (int i=0; i < 10000000; i++);

    unsigned crc = crc32(file, nbytes);
    trace("\nsend_file: sending %d bytes, crc32=%x\n", nbytes, crc);
    boot_output("waiting for a start\n");

    // unix step 1
    uint32_t val;
    while ((val = boot_get32(fd)) != GET_PROG_INFO)
        val = boot_get8(fd);

    boot_put32(fd, PUT_PROG_INFO);
    boot_put32(fd, file_addr);
    boot_put32(fd, nbytes);    
    boot_put32(fd, crc);

    // unix step 2
    while((val = boot_get32(fd)) != GET_CODE);
    
    if (val == BOOT_ERROR) {
        printf("error on pi side, exiting...\n");
        return;
    } else if (val != GET_CODE) {
        printf("weird output\n");
        return;
    }

    if (boot_get32(fd) != crc) {
        printf("crc sent from pi doesn't match...\n");
        return;
    }

    boot_put32(fd, PUT_CODE);
    for (unsigned i=0; i < nbytes; i++) {
        boot_put8(fd, *(file + i));
    }

    // unix step 3
    val = boot_get32(fd);
    if (val == BOOT_ERROR) {
        printf("error on pi side, exiting...\n");
        return;
    } else if (val != BOOT_SUCCESS) {
        printf("weird output\n");
        return;
    }

    boot_output("file send: Done.\n");
    
    return;
}