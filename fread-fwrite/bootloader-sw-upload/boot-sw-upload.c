#include "rpi.h"
#include "pi-sd.h"
#include "fat32.h"

void notmain() {
  kmalloc_init(FAT32_HEAP_MB);
  pi_sd_init();

  printk("Reading the MBR.\n");
  mbr_t *mbr = mbr_read();

  printk("Loading the first partition.\n");
  mbr_partition_ent_t partition;
  memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
  assert(mbr_part_is_fat32(partition.part_type));

  printk("Loading the FAT.\n");
  fat32_fs_t fs = fat32_mk(&partition);

  printk("Loading the root directory.\n");
  pi_dirent_t root = fat32_get_root(&fs);

  printk("Listing files:\n");
  uint32_t n;
  pi_directory_t files = fat32_readdir(&fs, &root);
  printk("Got %d files.\n", files.ndirents);
  for (int i = 0; i < files.ndirents; i++) {
    if (files.dirents[i].is_dir_p) {
      printk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
    } else {
      printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
    }
  }

  printk("getting clusters for kernel.img\n");
  char *filename = "KERNEL.IMG";

  uint32_t num_clusters;
  uint32_t buf[128];

  fat32_get_chain(&fs, &root, filename, buf, &num_clusters);

  uint32_t last_cluster = *(buf + 63);  // 0x200000 address of bootloader code
  uint32_t num_bytes = *((uint32_t *) 0x12000);
  printk("num bytes: %d\n", num_bytes);

  uint8_t *last_data = (uint8_t *) 0x12004;

  write_cluster_chain(&fs, last_cluster, last_data, num_bytes);
  write_fat_to_disk(&fs);
  fat32_update_file_size(&fs, &root, filename, num_bytes + 63 * 64 * 512);

  printk("\nListing files after write:\n");
  files = fat32_readdir(&fs, &root);
  printk("Got %d files.\n", files.ndirents);
  for (int i = 0; i < files.ndirents; i++) {
    if (files.dirents[i].is_dir_p) {
      printk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
    } else {
      printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
    }
  }
}
