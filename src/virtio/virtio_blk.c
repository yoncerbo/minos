#include "virtio_blk.h"

VirtioBlkdev virtio_blk_init(VirtioDevice *dev) {
  ASSERT(dev->magic == 0x74726976);
  ASSERT(dev->version == 1);
  ASSERT(dev->device = VIRTIO_DEVICE_BLK);

  dev->status = 0;
  dev->status |= VIRTIO_STATUS_ACK;
  dev->status |= VIRTIO_STATUS_DRIVER;
  dev->status |= VIRTIO_STATUS_FEAT_OK;

  Virtq *vq = virtq_create(dev, 0);
  dev->status |= VIRTIO_STATUS_DRIVER_OK;

  uint64_t capacity = *(uint64_t *)dev->config;
  printf("virtio: blkdev with capacity = %d\n", (uint32_t)capacity);

  return (VirtioBlkdev){
    .vq = vq,
    .sector_capacity = capacity,
    .dev = dev,
  };
}

void virtio_blk_rw(VirtioBlkdev *blkdev, uint8_t *buffer, uint32_t first_sector, uint32_t len, bool is_write) {
  // TODO: It should be an error
  ASSERT(first_sector + len <= blkdev->sector_capacity);

  blkdev->request = (VirtioBlkReq){
    .sector = first_sector,
    .type = is_write ? VIRTIO_BLK_OUT : VIRTIO_BLK_IN,
  };

  Virtq *vq = blkdev->vq;
  vq->descs[0] = (VirtqDesc){
    .addr = (uint32_t)&blkdev->request,
    .len = sizeof(blkdev->request),
    .next = 1,
    .flags = VIRTQ_DESC_NEXT,
  };
  vq->descs[1] = (VirtqDesc){
    .addr = (uint32_t)buffer,
    .len = SECTOR_SIZE * len,
    .next = 2,
    .flags = VIRTQ_DESC_NEXT | (is_write ? 0 : VIRTQ_DESC_WRITE),
  };
  vq->descs[2] = (VirtqDesc){
    .addr = (uint32_t)&blkdev->status,
    .len = 1,
    .flags = VIRTQ_DESC_WRITE,
  };

  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = 0;
  __sync_synchronize();
  blkdev->dev->queue_notify = 0;

  while (vq->avail.index != vq->used.index);

  // TODO: It should be an error
  ASSERT(blkdev->status == 0);
}
