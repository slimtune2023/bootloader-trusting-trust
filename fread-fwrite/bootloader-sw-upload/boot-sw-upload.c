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
  
  printk("chain of clusters for file: %s\n", filename);
  for (uint32_t i=0; i < num_clusters; i++) {
    printk("cluster %d: %d\n", i+1, *(buf + i));
  }
}
