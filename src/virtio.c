#include "common.h"

// Legacy virtio interface
// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-220004

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
} VirtioDevice;

