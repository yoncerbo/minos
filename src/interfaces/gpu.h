#ifndef INCLUDE_INTEFACES_GPU
#define INCLUDE_INTEFACES_GPU

#include "common.h"

// TODO: Rename it to GpuDev
typedef struct Gpu Gpu;

void gpu_v1_get_surface(Gpu *gpu, Surface *out_surface);
void gpu_v1_flush(Gpu *gpu);

#endif
