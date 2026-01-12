#ifndef INCLUDE_VIRTIO_GPU
#define INCLUDE_VIRTIO_GPU

#include "common.h"

#include "interfaces/gpu.h"

const uint32_t DISPLAY_WIDTH = 640;
const uint32_t DISPLAY_HEIGHT = 480;

typedef struct {
  uint8_t r, g, b, a;
} Rgba;

typedef struct Gpu {
  VirtioDevice *dev;
  Virtq *vq;
  Virtq *cq;
  uint32_t *fb;
} VirtioGpu;

#endif // !INCLUDE_VIRTIO_GPU
