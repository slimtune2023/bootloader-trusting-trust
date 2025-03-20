#ifndef __UNIX_BOOT_H__
#define __UNIX_BOOT_H__
#ifndef __RPI__
#   include "libunix.h"
#else
#   include "rpi.h"
#endif
#include "boot-defs.h"

enum { TRACE_FD = 21 };
enum { TRACE_CONTROL_ONLY = 1, TRACE_ALL = 2 };
extern int trace_p;

uint32_t get_uint32_trace(int fd);
uint32_t get_uint32_check_print(int fd);
void simple_boot(int fd, uint32_t addr, const uint8_t *buf, unsigned n);

#endif