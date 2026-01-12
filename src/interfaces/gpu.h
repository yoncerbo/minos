#ifndef INCLUDE_INTEFACES_GPU
#define INCLUDE_INTEFACES_GPU

#include "common.h"

typedef struct {
  uint32_t *ptr;
  uint32_t width, height, pitch;
} Surface;

typedef struct Gpu Gpu;

void gpu_v1_get_surface(Gpu *gpu, out Surface *texture);
void gpu_v1_flush(Gpu *gpu);

#endif
