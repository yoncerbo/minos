#ifndef INCLUDE_VIRTIO_NET
#define INCLUDE_VIRTIO_NET

#include "virtio.h"

typedef enum {
  VIRTIO_NET_F_MAC = 5,
} VirtioNetFeatures;

typedef struct {
  uint8_t mac[6];
  uint16_t status;
  uint16_t max_virtq_pairs;
  uint16_t mtu;
} PACKED VirtioNetConfig;

typedef struct {
  uint8_t flags;
  uint8_t gso_type;
  uint16_t hdr_len;
  uint16_t gso_size;
  uint16_t csum_start;
  uint16_t num_buffers;
} PACKED VirtioNetHeader;

typedef struct {
  VirtioDevice *dev;
  Virtq *rq;
  Virtq *tq;
  VirtioNetHeader header;
  uint8_t mac[6];
  uint32_t processed_requests;
  char *buffers;
} VirtioNetdev;

VirtioNetdev virtio_net_init(VirtioDevice *dev);
uint32_t virtio_net_recv(VirtioNetdev *netdev);
void virtio_net_return_buffer(VirtioNetdev *netdev, uint32_t index);

#endif
