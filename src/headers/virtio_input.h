#ifndef INCLUDE_VIRTIO_INPUT
#define INCLUDE_VIRTIO_INPUT

#include "common.h"
#include "virtio.h"

typedef struct PACKED {
  uint16_t type;
  uint16_t code;
  uint32_t value;
} VirtioInputEvent;

typedef struct {
  VirtioDevice *dev;
  Virtq *event_queue;
  Virtq *status_queue;
  VirtioInputEvent *events;
  uint16_t processed;
} VirtioInput;

VirtioInput virtio_input_init(VirtioDevice *dev);
bool virtio_input_get(VirtioInput *input, VirtioInputEvent *ev);

#endif

