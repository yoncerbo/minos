#ifndef INCLUDE_VIRTIO_INPUT
#define INCLUDE_VIRTIO_INPUT

#include "common.h"
#include "interfaces/input.h"
#include "virtio.h"

typedef struct InputDev {
  VirtioDevice *dev;
  Virtq *event_queue;
  Virtq *status_queue;
  InputEvent *events;
  uint16_t processed;
} VirtioInput;

VirtioInput virtio_input_init(VirtioDevice *dev);

#endif

