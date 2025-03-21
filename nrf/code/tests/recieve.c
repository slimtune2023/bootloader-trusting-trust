// ping pong 4-byte packets back and forth.
#include "nrf-test.h"

// useful to mess around with these. 
enum { ntrial = 1000, timeout_usec = 1000, nbytes = 4 };

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

// send 4 byte packets from <server> to <client>.  
//
// nice thing about loopback is that it's trivial to know what we are 
// sending, whether it got through, and do flow-control to coordinate
// sender and receiver.
static void 
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

    uint32_t buf[0x10000];
    for (int i = 0; i < nmsg; i++) {
        while(!net_get32(c, &got)) {
            // printk("lost packet\n");
            ;
        }
        *(buf + i) = got;
        // printk("got packet\n");
    }

    printk("Recieved message from client: %s\n\n", (char *)buf);
}

void notmain(void) {
    kmalloc_init(1);

    // configure server
    // trace("send total=%d, %d-byte messages from server=[%x] to client=[%x]\n",
                // ntrial, nbytes, server_addr, client_addr);

    nrf_t *c = client_mk_ack(client_addr, nbytes);

    nrf_stat_start(c);

    // run test.
    recieve(c);

    // emit all the stats.
}
