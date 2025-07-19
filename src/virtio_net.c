#include "common.h"

// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-1940001

typedef enum {
  VIRTIO_NET_F_MAC = 5,
} ViritoNetFeatures;

typedef struct {
  uint8_t mac[6];
  uint16_t status;
  uint16_t max_virtq_pairs;
  uint16_t mtu;
} __attribute__((packed)) VirtioNetConfig;

typedef struct {
  uint8_t flags;
  uint8_t gso_type;
  uint16_t hdr_len;
  uint16_t gso_size;
  uint16_t csum_start;
  uint16_t num_buffers;
} __attribute__((packed)) VirtioNetHeader;

typedef struct {
  VirtioDevice *dev;
  Virtq *rq;
  Virtq *tq;
  VirtioNetHeader header;
  uint8_t mac[6];
} VirtioNetdev;

VirtioNetdev virtio_net_init(VirtioDevice *dev) {
  VirtioNetdev netdev = {
    .dev = dev,
  };
  ASSERT(dev->magic == 0x74726976);
  ASSERT(dev->version == 1);
  ASSERT(dev->device = VIRTIO_DEVICE_NET);

  dev->status = 0;
  dev->status |= VIRTIO_STATUS_ACK;
  dev->status |= VIRTIO_STATUS_DRIVER;

  // Will this work?
  uint32_t features = dev->host_features;
  ASSERT(features & VIRTIO_NET_F_MAC);
  dev->guest_features = VIRTIO_NET_F_MAC;
  dev->status |= VIRTIO_STATUS_FEAT_OK;

  uint8_t *mac = (void *)dev->config;
  printf("MAC: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1],
      mac[2], mac[3], mac[4], mac[5]);

  Virtq *rq = virtq_create(dev, 0);
  Virtq *tq = virtq_create(dev, 1);

  // 3. Fill the receive queues with buffers 5.1.6.3
  // 5. if you don't have mac addres, generate a random one
  
  dev->status |= VIRTIO_STATUS_DRIVER_OK;

  netdev.rq = rq;
  netdev.tq = tq;
  memcpy(&netdev.mac, mac, 6);
  return netdev;
}

void virtio_net_send(VirtioNetdev *netdev, char *packet, uint32_t size) {
  // Send completely checksumed packet
  // flags zero, gso_type = hdr_gso_none
  // the header and packet are added as one output descriptor
  
  netdev->header = (VirtioNetHeader){
    .num_buffers = 0,
    .flags = 0,
    .gso_type = 0,
  };
  
  Virtq *tq = netdev->tq;
  tq->descs[0] = (VirtqDesc){
    .addr = (uint32_t)&netdev->header,
    .len = sizeof(netdev->header),
    .next = 1,
    .flags = VIRTQ_DESC_NEXT,
  };
  tq->descs[1] = (VirtqDesc){
    .addr = (uint32_t)packet,
    .len = size,
  };

  tq->avail.ring[tq->avail.index++ % VIRTQ_ENTRY_NUM] = 0;
  __sync_synchronize();
  netdev->dev->queue_notify = 1;

  // TODO: get rid of it when we have some kind of task system
  while (tq->avail.index != tq->used.index);
}
