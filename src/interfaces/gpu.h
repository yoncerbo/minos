#ifndef INCLUDE_INTEFACES_GPU
#define INCLUDE_INTEFACES_GPU

#include "common.h"

// TODO: Rename it to GpuDev
typedef struct GpuDev GpuDev;

void gpu_v1_get_surface(GpuDev *gpu, Surface *out_surface);
void gpu_v1_flush(GpuDev *gpu);

#endif
