#ifndef INCLUDE_FAT
#define INCLUDE_FAT

#include "common.h"
#include "virtio_blk.h"
#include "vfs.h"

typedef struct {
  Fs fs; // every file system driver needs this header
  uint8_t *buffer;
  VirtioBlkdev *blkdev;
  uint32_t first_data_sector;
  uint32_t first_fat_sector;
  uint32_t root_cluster;
  uint8_t sectors_per_cluster;
} FatDriver;

FatDriver fat_driver_init(VirtioBlkdev *blkdev);
DirEntry fat_find_directory_entry(FatDriver *driver, uint32_t first_directory_cluster, Str name);
void fat_rw_sectors(FatDriver *driver, uint32_t first_cluster, uint32_t sectors_start, uint32_t sectors_len, uint8_t *buffer, bool is_write);

#endif
