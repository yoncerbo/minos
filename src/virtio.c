#include "common.h"

// Legacy virtio interface
// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-220004

#define SECTOR_SIZE 512

typedef enum {
  VIRTIO_DEVICE_BLK = 2,
} VirtioDeviceType;

typedef enum {
  VIRTIO_STATUS_ACK = 1 << 0,
  VIRTIO_STATUS_DRIVER = 1 << 1,
  VIRTIO_STATUS_DRIVER_OK = 1 << 2,
  VIRTIO_STATUS_FEAT_OK = 1 << 3,
} VirtioDeviceStatus;

typedef volatile struct {
  uint32_t magic;
  uint32_t version;
  uint32_t device;
  uint32_t vendor;
  uint32_t host_features;
  uint32_t host_features_sel;
  char _padding1[8];
  uint32_t guest_features;
  uint32_t guest_features_sel;
  uint32_t guest_page_size;
  char _padding2[4];
  uint32_t queue_sel;
  uint32_t queue_num_max;
  uint32_t queue_num;
  uint32_t queue_align;
  uint32_t queue_pfn;
  char _padding3[12];
  uint32_t queue_notify;
  char _padding4[12];
  uint32_t interrupt_status;
  uint32_t interrupt_ack;
  char _padding5[8];
  uint32_t status;
  char _padding6[140];
  char config[];
} VirtioDevice;

// TODO: 128 would be the bigges power of 2,
// that make our virtq still fit inside 2 pages
#define VIRTQ_ENTRY_NUM 16

typedef enum {
  VIRTQ_DESC_NEXT = 1,
  VIRTQ_DESC_WRITE = 2,
} VirtqDescFlags;

typedef struct {
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
} __attribute__((packed)) VirtqDesc;

typedef struct {
  uint16_t flags;
  uint16_t index;
  uint16_t ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed)) VirtqAvail;

typedef struct {
  uint32_t id;
  uint32_t len;
} __attribute__((packed)) VirtqUsedElem;

typedef struct {
  uint16_t flags;
  uint16_t index;
  VirtqUsedElem ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed)) VirtqUsed;

typedef volatile struct {
  VirtqDesc descs[VIRTQ_ENTRY_NUM];
  VirtqAvail avail;
  VirtqUsed used __attribute__((aligned(PAGE_SIZE)));
} __attribute__((packed)) Virtq;

typedef enum {
  VIRTIO_BLK_IN = 0, // read
  VIRTIO_BLK_OUT = 1, // write
} VirtioBlkReqType;

typedef struct {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
} __attribute__((packed)) VirtioBlkReq;

typedef struct {
  VirtioDevice *dev;
  Virtq *vq;
  uint32_t sector_capacity;
  VirtioBlkReq request;
  uint8_t status;
} VirtioBlkdev;


VirtioBlkdev virtio_blk_init(VirtioDevice *dev) {
  ASSERT(dev->magic == 0x74726976);
  ASSERT(dev->version == 1);
  ASSERT(dev->device = VIRTIO_DEVICE_BLK);

  dev->status = 0;
  dev->status |= VIRTIO_STATUS_ACK;
  dev->status |= VIRTIO_STATUS_DRIVER;
  dev->status |= VIRTIO_STATUS_FEAT_OK;

  Virtq *vq = (void *)alloc_pages(2);
  dev->queue_sel = 0;
  dev->queue_num = VIRTQ_ENTRY_NUM;
  dev->queue_align = 0;
  dev->queue_pfn = (uint32_t)vq;

  dev->status |= VIRTIO_STATUS_DRIVER_OK;

  uint64_t capacity = *(uint64_t *)dev->config;
  printf("virtio: blkdev with capacity = %d\n", (uint32_t)capacity);

  return (VirtioBlkdev){
    .vq = vq,
    .sector_capacity = capacity,
    .dev = dev,
  };
}

void virtio_blk_rw(VirtioBlkdev *blkdev, char *buffer, uint32_t first_sector, uint32_t len, bool is_write) {
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
