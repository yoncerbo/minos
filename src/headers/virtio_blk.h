#ifndef INCLUDE_VIRTIO_BLK
#define INCLUDE_VIRTIO_BLK

#include "interfaces/blk.h"
#include "virtio.h"

typedef enum {
  VIRTIO_BLK_IN = 0, // read
  VIRTIO_BLK_OUT = 1, // write
} VirtioBlkReqType;

typedef struct PACKED {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
} VirtioBlkReq;

typedef struct BlkDev {
  VirtioDevice *dev;
  Virtq *vq;
  uint32_t sector_capacity;
  VirtioBlkReq request;
  uint8_t status;
} VirtioBlkdev;

VirtioBlkdev virtio_blk_init(VirtioDevice *dev);
void virtio_blk_rw(VirtioBlkdev *blkdev, uint8_t *buffer, uint32_t first_sector, uint32_t len, bool is_write);

#endif
