#include "rpi.h"
#include "pi-sd.h"
#include "fat32.h"
#include "nrf-test.h"

enum { ntrial = 1000, timeout_usec = 1000, nbytes = 4 };

static char *buf;
static uint32_t offset = 0;

// example possible wrapper to recv a 32-bit value.
static int net_get32(nrf_t *nic, uint32_t *out) {
  int ret = nrf_read_exact_timeout(nic, out, 4, timeout_usec);
  if(ret != 4) {
      debug("receive failed: ret=%d\n", ret);
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

void fputchar(const char x) {
  *(buf + offset) = x;
  offset++;
}

static void femit_val(unsigned base, uint32_t u) {
  char num[33], *p = num;

  switch(base) {
  case 2:
      do {
          *p++ = "01"[u % 2];
      } while(u /= 2);
      break;
  case 10:
      do {
          *p++ = "0123456789"[u % 10];
      } while(u /= 10);
      break;
  case 16:
      do {
          *p++ = "0123456789abcdef"[u % 16];
      } while(u /= 16);
      break;
  default: 
      panic("invalid base=%d\n", base);
  }

  // buffered in reverse, so emit backwards
  while(p > &num[0]) {
      p--;
      fputchar(*p);
  }
}

// a really simple printk. 
// need to do <sprintk>
int fvprintk(const char *fmt, va_list ap) {
  for(; *fmt; fmt++) {
      if(*fmt != '%')
          fputchar(*fmt);
      else {
          fmt++; // skip the %

          uint32_t u;
          int v;
          char *s;

          switch(*fmt) {
          case 'b': femit_val(2, va_arg(ap, uint32_t)); break;
          case 'u': femit_val(10, va_arg(ap, uint32_t)); break;
          case 'c': fputchar(va_arg(ap, int)); break;

          // we only handle %llx.   
          case 'l':  
              fmt++;
              if(*fmt != 'l')
                  panic("only handling llx format, have: <%s>\n", fmt);
              fmt++;
              if(*fmt != 'x')
                  panic("only handling llx format, have: <%s>\n", fmt);
              fputchar('0');
              fputchar('x');
              // have to get as a 64 vs two 32's b/c of arg passing.
              uint64_t x = va_arg(ap, uint64_t);
              // we break it up otherwise gcc will emit a divide
              // since it doesn't seem to strength reduce uint64_t
              // on a 32-bit machine.
              uint32_t hi = x>>32;
              uint32_t lo = x;
              if(hi)
                  femit_val(16, hi);
              femit_val(16, lo);
              break;

          // leading 0x
          case 'x':  
          case 'p': 
              fputchar('0');
              fputchar('x');
              femit_val(16, va_arg(ap, uint32_t));
              break;
          // print '-' if < 0
          case 'd':
              v = va_arg(ap, int);
              if(v < 0) {
                  fputchar('-');
                  v = -v;
              }
              femit_val(10, v);
              break;
          // string
          case 's':
              for(s = va_arg(ap, char *); *s; s++) 
                  fputchar(*s);
              break;
          default: panic("bogus identifier: <%c>\n", *fmt);
          }
      }
  }
  return 0;
}

int fprintk(const char *fmt, ...) {
  va_list args;

  int ret;
  va_start(args, fmt);
     ret = fvprintk(fmt, args);
  va_end(args);
  return ret;
}

void notmain() {
  kmalloc_init(FAT32_HEAP_MB);
  pi_sd_init();

  // printk("Reading the MBR.\n");
  mbr_t *mbr = mbr_read();

  // printk("Loading the first partition.\n");
  mbr_partition_ent_t partition;
  memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
  assert(mbr_part_is_fat32(partition.part_type));

  // printk("Loading the FAT.\n");
  fat32_fs_t fs = fat32_mk(&partition);

  char arr[1000];
  memset(arr, 0, 1000);

  buf = arr;
  offset = 0;

  fprintk("Loading the root directory.\n");

  pi_dirent_t root = fat32_get_root(&fs);

  fprintk("Listing files:\n");
  uint32_t n;
  pi_directory_t files = fat32_readdir(&fs, &root);
  fprintk("Got %d files.\n", files.ndirents);
  for (int i = 0; i < files.ndirents; i++) {
    if (files.dirents[i].is_dir_p) {
      fprintk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
    } else {
      fprintk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
    }
  }

  fprintk("\nLooking for secret.txt.\n");
  char *name = "SECRET.TXT";
  pi_dirent_t *config = fat32_stat(&fs, &root, name);
  demand(config, secret.txt not found!\n);

  fprintk("Reading secret.txt.\n");
  pi_file_t *file = fat32_read(&fs, &root, name);

  fprintk("Printing secret.txt (%d bytes):\n", file->n_data);
  fprintk("--------------------\n");
  for (int i = 0; i < file->n_data; i++) {
    fprintk("%c", file->data[i]);
  }
  fprintk("--------------------\n");

  printk("size of output to send: %d bytes\n", offset);
  printk("%s\n", buf);

  nrf_t *s = server_mk_ack(server_addr, nbytes);
  printk("server address: %x\n", server_addr);
  printk("client address: %x\n", client_addr);
  
  nrf_stat_start(s);

  uint32_t num_messages = (offset / 4) + 1;
  net_put32(s, client_addr, num_messages);

  uint32_t *to_send = (uint32_t *) buf;
  for (uint32_t i = 0; i < num_messages; i++) {
    net_put32(s, client_addr, *(to_send + i));
  }
}
