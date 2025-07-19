#include "common.h"

// Legacy virtio interface
// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-220004

#define SECTOR_SIZE 512

typedef enum {
  VIRTIO_DEVICE_NET = 1,
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
  uint32_t host_features; // device features
  uint32_t host_features_sel;
  char _padding1[8];
  uint32_t guest_features; // driver features
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

Virtq *virtq_create(VirtioDevice *dev, uint32_t index) {
  Virtq *vq = (void *)alloc_pages(2);
  dev->queue_sel = index;
  dev->queue_num = VIRTQ_ENTRY_NUM;
  dev->queue_align = 0;
  dev->queue_pfn = (uint32_t)vq;
  return vq;
}
