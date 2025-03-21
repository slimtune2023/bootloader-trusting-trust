#include "rpi.h"
#include "mmu.h"
#include "full-except.h"
#include "asm-helpers.h"

#include "nrf-test.h"

// useful to mess around with these. 
enum { ntrial = 1000, timeout_usec = 1000, nbytes_nrf = 4 };

// example possible wrapper to recv a 32-bit value.
static int net_get32(nrf_t *nic, uint32_t *out) {
    int ret = nrf_read_exact_timeout(nic, out, 4, timeout_usec);
    if(ret != 4) {
        // debug("receive failed: ret=%d\n", ret);
        return 0;
    }
    return 1;
}
// example possible wrapper to send a 32-bit value.
static void net_put32(nrf_t *nic, uint32_t txaddr, uint32_t x) {
    int ret = nrf_send_ack(nic, txaddr, &x, 4);
    if(ret != 4)
        panic("ret=%d, expected 4\n");
}

#define MB(x) ((x)*1024*1024)

#define RESERVED 0x6000000

enum {
    AUX_BASE = 0x20210000,
    AUX_IO = AUX_BASE + 0x5040,
    AUX_STAT = AUX_BASE + 0x5064,
};

enum {
    SEG_CODE = MB(0),
    SEG_HEAP = MB(1),
    SEG_STACK = STACK_ADDR - MB(1),
    SEG_INT_STACK = INT_STACK_ADDR - MB(1),
    SEG_BCM_0 = 0x20000000,
    SEG_BCM_1 = SEG_BCM_0 + MB(1),
    SEG_BCM_2 = SEG_BCM_0 + MB(2),
    SEG_ILLEGAL = MB(2),
};

cp_asm_get(cp15_fault_addr, p15, 0, c6, c0, 0);
cp_asm_get(cp15_dfsr, p15, 0, c5, c0, 0);

static nrf_t n;

static uint32_t * 
recieve(nrf_t *c) {
    unsigned client_addr = c->rxaddr;
    printk("*** client address: %x ***\n\n", client_addr);
    
    // server to client
    uint32_t got;
    uint32_t iter = 0;
    while(!net_get32(c, &got)) {
        if (iter % 1000 == 0)
            printk("waiting... %d\n", iter);
        // delay_ms(10);
        iter++;
    }
    uint32_t nmsg = got;

    printk("nmsg: %x\n", nmsg);

    uint32_t *buf = kmalloc(0x10000);
    for (int i = 0; i < nmsg; i++) {
        while(!net_get32(c, &got)) {
            // printk("lost packet\n");
            ;
        }
        *(buf + i) = got;
        // printk("got packet\n");
    }

    return buf;
}

void cleanup(void) {
    // mmu_disable();
    assert(!mmu_is_enabled());

    printk("-");

    uint32_t nbytes = *(uint32_t *)RESERVED;
    char *buf = (char *)(RESERVED + 4);
    printk("\n-----------------\nRECIEVED MESSAGE (nbytes = %x): \n", nbytes);
    for (int i = 0; i < nbytes; i++) {
        printk("%c", buf[i]);
    }

    nrf_t *c = client_mk_ack(&n, client_addr, nbytes_nrf);
    nrf_stat_start(c);
    uint32_t *buffer = recieve(c);

    printk("RUNNING PROG\n");

    uint32_t buf_size = sizeof(buffer);

    delay_ms(100);

    memcpy((void *)0x8000, buffer, buf_size);

    printk("first word at 0x8000 = %x\n", *(uint32_t *)0x8000);

    delay_ms(100);

    void (*code_fn)(void) = (void (*)(void)) 0x8000;
    code_fn();

    // this triggers done
    printk("\n-----------------\n\n");
    printk("SUCCESS!!\n");

    uart_flush_tx();
    delay_ms(10);

    // rpi_reboot();
}

// Fault handler to verify exception details
static void data_abort_handler(regs_t *r) {
    mmu_disable();

    // printk("data abort\n");
    uint32_t fault_addr = cp15_fault_addr_get();
    uint32_t dfsr = cp15_dfsr_get();
    uint32_t write = (dfsr >> 11) & 1;

    uint32_t pc = r->regs[15];

    r->regs[15]+=4;

    // printk("FAULT: PC = %x, Fault Address = %x, DFSR = %x, WRITE = %x\n", pc, fault_addr, dfsr, write);

    // enum { no_user = perm_rw_priv };

    if (fault_addr == AUX_STAT && !write) {
        do {
            r->regs[0] = GET32(AUX_STAT);
        } while (!(GET32(AUX_STAT) & 2));
    } else if (fault_addr == AUX_IO && write) {
        printk("%c", (char)r->regs[1]);
        uint32_t ct = *(uint32_t *)RESERVED;
        *(char *)(RESERVED + 4 + ct) = (char)r->regs[1];
        (*(uint32_t *)RESERVED) ++;

        if (ct >= 6) {
            if (strncmp((char *)(RESERVED + 4 + ct - 6), "DONE!!!", 7) == 0) {
                cleanup();
                switchto(r);
            }
        }
    } else if (fault_addr == 0x20215064 && !write) {
        // dev barrier
        dev_barrier();
    } else {
        // printk("FAULT: PC = %x, Fault Address = %x, DFSR = %x, WRITE = %x\n", pc, fault_addr, dfsr, write);
    }

    mmu_enable();

    switchto(r);
}

void notmain(void) {
    printk("In handler!\n");
    full_except_install(0);
    full_except_set_data_abort(data_abort_handler);
}