#include "common.h"
#include "virtio_input.h"
#include "interfaces/input.h"
#include "memory.h"
#include "virtio.h"

#include "hardware.h"

enum {
  VIRTIO_INPUT_CFG_ID_NAME = 1,
  VIRTIO_INPUT_CFG_EV_BITS = 0x11,
  VIRTIO_INPUT_CFG_ABS_INFO = 0x12,
};

typedef volatile struct {
  uint8_t select;
  uint8_t subsel;
  uint8_t size;
  uint8_t reserved[5];
  union {
    char string[128];
    uint8_t bitmap[128];
    InputAbsInfo abs_info;
  } u;
} VirtioInputConfig;

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
      .addr = (uint32_t)buffer + sizeof(InputEvent) * i,
      .len = sizeof(InputEvent),
      .flags = VIRTQ_DESC_WRITE,
    };
    eq->avail.ring[i] = i;
  }
  eq->avail.index += 16;
  __sync_synchronize();

  dev->status |= VIRTIO_STATUS_DRIVER_OK;

  return (VirtioInput){
    .dev = dev,
    .event_queue = eq,
    .status_queue = sq,
    .events = (void *)buffer,
  };
}

uint32_t input_v1_read_events(InputDev *dev, InputEvent *out_events, uint32_t event_limit) {
  Virtq *vq = dev->event_queue;
  uint32_t available_events = vq->used.index - dev->processed;
  if (available_events <= 0) return 0;

  uint32_t events_to_write = UPPER_BOUND(available_events, event_limit);
  for (uint32_t i = 0; i < events_to_write; ++i) {
    VirtqUsedElem elem = vq->used.ring[dev->processed++ % VIRTQ_ENTRY_NUM];
    // ASSERT(elem.len == sizeof(InputEvent));
    out_events[i] = dev->events[elem.id];
    vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = elem.id;
  }

  __sync_synchronize();
  dev->dev->queue_notify = 0;
  return events_to_write;
}

uint32_t input_v1_get_name(InputDev *dev, char *buffer, uint32_t limit) {
  VirtioInputConfig *config = (void *)dev->dev->config;
  config->select = VIRTIO_INPUT_CFG_ID_NAME;
  config->subsel = 0;

  uint32_t size = config->size;
  if (!size) return 0;

  uint32_t to_write = UPPER_BOUND(size, limit);
  memcpy(buffer, config->u.string, to_write);
  return to_write;
}

uint32_t input_v1_get_capabilities(InputDev *dev, EventType type, uint8_t *buffer, uint32_t limit) {
  VirtioInputConfig *config = (void *)dev->dev->config;
  config->select = VIRTIO_INPUT_CFG_EV_BITS;
  config->subsel = type;

  uint32_t size = config->size;
  if (!size) return 0;

  uint32_t to_write = UPPER_BOUND(size, limit);
  memcpy(buffer, config->u.bitmap, to_write);
  return to_write;
}

void input_v1_get_abs_info(InputDev *dev, uint8_t axis, InputAbsInfo *out_abs_info) {
  VirtioInputConfig *config = (void *)dev->dev->config;
  config->select = VIRTIO_INPUT_CFG_ABS_INFO;
  config->subsel = axis;

  ASSERT(config->size == sizeof(InputAbsInfo));
  *out_abs_info = config->u.abs_info;
}
