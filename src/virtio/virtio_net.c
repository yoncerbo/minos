#include "virtio_net.h"
#include "virtio.h"

// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-1940001

VirtioNetdev virtio_net_init(VirtioDevice *dev) {
  ASSERT(dev->magic == VIRTIO_MAGIC);
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
  Virtq *rq = virtq_create(dev, 0);
  Virtq *tq = virtq_create(dev, 1);

  char *buffers = (void *)alloc_pages(8);
  uint32_t buffer_size = 2048;
  for (uint32_t i = 0; i < 16; ++i) {
    rq->descs[i] = (VirtqDesc){
      .addr = (uint32_t)buffers + buffer_size * i,
      .len = buffer_size,
      .flags = VIRTQ_DESC_WRITE,
    };
    rq->avail.ring[i] = i;
  }
  rq->avail.index += 16;
  __sync_synchronize();
  // dev->queue_notify = 0;

  dev->status |= VIRTIO_STATUS_DRIVER_OK;

  VirtioNetdev netdev = {
    .dev = dev,
    .buffers = buffers,
    .rq = rq,
    .tq = tq,
  };
  memcpy(&netdev.mac, mac, 6);
  return netdev;
}

void virtio_net_send(VirtioNetdev *netdev, uint8_t *packet, uint32_t size) {
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

// TODO: I don't really like the way it works, I'm gonna change it later
// Returns the buffer index - 0..16
uint32_t virtio_net_recv(VirtioNetdev *netdev) {
  while (netdev->rq->used.index <= netdev->processed_requests) {
    LOG("Waiting\n");
  }
  LOG("Packets available\n");

  uint32_t used_index = netdev->processed_requests++;
  LOG("Processing request %d\n", used_index);
  VirtqUsedElem elem = netdev->rq->used.ring[used_index % VIRTQ_ENTRY_NUM];

  volatile VirtqDesc *desc = &netdev->rq->descs[elem.id];
  // TODO: why the addr is zero?
  // We populated the queue, so that each descriptor chain
  // has only one link
  ASSERT((desc->flags & VIRTQ_DESC_NEXT) == 0);
  ASSERT(desc->len == 2048);

  return elem.id;
}

void virtio_net_return_buffer(VirtioNetdev *netdev, uint32_t index) {
  Virtq *tq = netdev->tq;
  tq->avail.ring[tq->avail.index++ & VIRTQ_ENTRY_NUM] = index;
  // Is it needed?
  __sync_synchronize();
  netdev->dev->queue_notify = 0;
}
