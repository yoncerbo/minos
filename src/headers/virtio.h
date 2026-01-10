#ifndef INCLUDE_VIRTIO
#define INCLUDE_VIRTIO

// Legacy virtio interface
// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-220004

#include "common.h"

const uint32_t VIRTIO_MAGIC = 0x74726976;

typedef enum {
  VIRTIO_DEVICE_NONE = 0,
  VIRTIO_DEVICE_NET = 1,
  VIRTIO_DEVICE_BLK = 2,
  VIRTIO_DEVICE_CONSOLE = 3,
  VIRTIO_DEVICE_GPU = 16,
  VIRTIO_DEVICE_INPUT = 18,
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
  char padding1[8];
  uint32_t guest_features; // driver features
  uint32_t guest_features_sel;
  uint32_t guest_page_size;
  char padding2[4];
  uint32_t queue_sel;
  uint32_t queue_num_max;
  uint32_t queue_num;
  uint32_t queue_align;
  uint32_t queue_pfn;
  char padding3[12];
  uint32_t queue_notify;
  char padding4[12];
  uint32_t interrupt_status;
  uint32_t interrupt_ack;
  char padding5[8];
  uint32_t status;
  char padding6[140];
  char config[];
} VirtioDevice;

// TODO: 128 would be the bigges power of 2,
// that make our virtq still fit inside 2 pages
#define VIRTQ_ENTRY_NUM 16

typedef enum {
  VIRTQ_DESC_NEXT = 1,
  VIRTQ_DESC_WRITE = 2,
} VirtqDescFlags;

typedef struct PACKED {
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
} VirtqDesc;

typedef struct PACKED {
  uint16_t flags;
  uint16_t index;
  uint16_t ring[VIRTQ_ENTRY_NUM];
} VirtqAvail;

typedef struct PACKED {
  uint32_t id;
  uint32_t len;
} VirtqUsedElem;

typedef struct PACKED {
  uint16_t flags;
  uint16_t index;
  VirtqUsedElem ring[VIRTQ_ENTRY_NUM];
} VirtqUsed;

typedef struct PACKED {
  VirtqDesc descs[VIRTQ_ENTRY_NUM];
  VirtqAvail avail;
  volatile VirtqUsed used ALIGNED(PAGE_SIZE);

  // helper variables for constructing descriptor chains
  uint32_t desc_index;
} Virtq;

Virtq *virtq_create(VirtioDevice *dev, uint32_t index);
inline void virtq_descf(Virtq *vq, void *addr, uint16_t len, bool is_write);
inline void virtq_descm(Virtq *vq, void *addr, uint16_t len, bool is_write);
inline void virtq_descl(Virtq *vq, void *addr, uint16_t len, bool is_write);

#endif
