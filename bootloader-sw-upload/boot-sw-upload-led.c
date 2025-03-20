#include "rpi.h"
#include "pi-sd.h"
#include "fat32.h"

void notmain() {
  kmalloc_init(FAT32_HEAP_MB);
  pi_sd_init();

  mbr_t *mbr = mbr_read();

  mbr_partition_ent_t partition;
  memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
  assert(mbr_part_is_fat32(partition.part_type));

  fat32_fs_t fs = fat32_mk(&partition);

  pi_dirent_t root = fat32_get_root(&fs);

  char *filename = "KERNEL.IMG";

  uint32_t num_clusters;
  uint32_t buf[128];

  fat32_get_chain(&fs, &root, filename, buf, &num_clusters);

  uint32_t last_cluster = *(buf + 63);  // 0x200000 address of bootloader code
  uint32_t num_bytes = *((uint32_t *) 0x12000);

  uint8_t *last_data = (uint8_t *) 0x12004;

  write_cluster_chain(&fs, last_cluster, last_data, num_bytes);
  write_fat_to_disk(&fs);
  fat32_update_file_size(&fs, &root, filename, num_bytes + 63 * 64 * 512);

  printk("Hello, world!\n");

  int led = 20;
    gpio_set_output(led);
    for(int i = 0; i < 10; i++) {
        gpio_set_on(led);
        delay_cycles(1000000);
        gpio_set_off(led);
        delay_cycles(1000000);
    }

    // last iter = leave the LED on so know to power-cycle
    gpio_set_on(led);
}
