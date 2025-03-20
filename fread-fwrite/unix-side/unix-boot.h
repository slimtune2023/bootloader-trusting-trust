#ifndef __UNIX_BOOT_H__
#define __UNIX_BOOT_H__
#ifndef __RPI__
#   include "libunix.h"
#else
#   include "rpi.h"
#endif
#include "boot-defs.h"

// hack to trace put/get.
// if 
//  <trace_p> = 1 
// you can see the stream of put/gets: makes it easy 
// to compare your bootloader to the ours and others.
//
// there are other ways to do this --- this is
// clumsy, but simple.
//
// NOTE: we could (but don't) intercept puts/gets transparently
// by interposing on a file, a socket or giving the my-install
// a file descriptor to use.

#define boot_output(msg...) output("BOOT:" msg)


enum { TRACE_FD = 21 };
enum { TRACE_CONTROL_ONLY = 1, TRACE_ALL = 2 };
extern int trace_p;

void boot_put8(int fd, uint8_t val);
void boot_put32(int fd, uint32_t val);
uint8_t boot_get8(int fd);
uint32_t boot_get32(int fd);

// <addr> = pi address to put the code at [buf, buf+n).
void simple_boot(int fd, uint32_t boot_addr, const uint8_t *code, unsigned nbytes);
void send_file(int fd, uint32_t file_addr, const uint8_t *file, unsigned nbytes, char *filename);

#endif
