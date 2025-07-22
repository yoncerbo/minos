#include "common.h"
#include "virtio_input.h"
#include "hardware.h"
#include "memory.h"
#include "virtio.h"

VirtioInput virtio_input_init(VirtioDevice *dev) {
  ASSERT(dev->magic == VIRTIO_MAGIC);
  ASSERT(dev->version == 1);
  ASSERT(dev->device = VIRTIO_DEVICE_INPUT);

  dev->status = 0;
  dev->status |= VIRTIO_STATUS_ACK;
  dev->status |= VIRTIO_STATUS_DRIVER;
  dev->status |= VIRTIO_STATUS_FEAT_OK;

  Virtq *eq = virtq_create(dev, 0);
  Virtq *sq = virtq_create(dev, 1);

  uint8_t *buffer = (void *)alloc_pages(1);

  for (uint32_t i = 0; i < 16; ++i) {
    eq->descs[i] = (VirtqDesc){
      .addr = (uint32_t)buffer + sizeof(VirtioInputEvent) * i,
      .len = sizeof(VirtioInputEvent),
      .flags = VIRTQ_DESC_WRITE,
    };
    eq->avail.ring[i] = i;
  }
  eq->avail.index += 16;
  __sync_synchronize();

  dev->status |= VIRTIO_STATUS_DRIVER_OK;

  return (VirtioInput){
    .dev = dev,
    .eq = eq,
    .sq = sq,
    .events = (void *)buffer,
  };
}

bool virtio_input_get(VirtioInput *input, VirtioInputEvent *ev) {
  Virtq *vq = input->eq;
  if (vq->used.index <= input->processed) return false;

  VirtqUsedElem elem = vq->used.ring[input->processed++ % VIRTQ_ENTRY_NUM];
  ASSERT(elem.len == sizeof(VirtioInputEvent));
  *ev = input->events[elem.id];

  vq->avail.ring[vq->avail.index++ & VIRTQ_ENTRY_NUM] = elem.id;
  __sync_synchronize();
  input->dev->queue_notify = 0;
  return true;
}
