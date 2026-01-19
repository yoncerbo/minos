#ifndef INCLUDE_INTERFACE_BLK
#define INCLUDE_INTERFACE_BLK

#include "common.h"

#define SECTOR_SIZE 512

typedef struct BlkDev BlkDev;

typedef enum {
  BLKDEV_READ = 0,
  BLKDEV_WRITE = 1,
} BlkDevFlags;

void blk_v1_read_write_sectors(BlkDev *blkdev, void *buffer, uint32_t first_sector, uint32_t len, BlkDevFlags flags);

#endif
