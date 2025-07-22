#include "common.h"
#include "memory.h"
#include "virtio.h"
#include "virtio_gpu.h"

const uint32_t PIXEL_FORMAT = 67; // RGBA

typedef struct PACKED {
  uint32_t events_read;
  uint32_t events_clear;
  uint32_t num_scanouts;
  uint32_t reserved;
} VirtioGpuConfig;

typedef enum {
  /* 2d commands */ 
  VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100, 
  VIRTIO_GPU_CMD_RESOURCE_CREATE_2D, 
  VIRTIO_GPU_CMD_RESOURCE_UNREF, 
  VIRTIO_GPU_CMD_SET_SCANOUT, 
  VIRTIO_GPU_CMD_RESOURCE_FLUSH, 
  VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D, 
  VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING, 
  VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING, 
  VIRTIO_GPU_CMD_GET_CAPSET_INFO, 
  VIRTIO_GPU_CMD_GET_CAPSET, 
  VIRTIO_GPU_CMD_GET_EDID, 
  
  /* cursor commands */ 
  VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300, 
  VIRTIO_GPU_CMD_MOVE_CURSOR, 
  
  /* success responses */ 
  VIRTIO_GPU_RESP_OK_NODATA = 0x1100, 
  VIRTIO_GPU_RESP_OK_DISPLAY_INFO, 
  VIRTIO_GPU_RESP_OK_CAPSET_INFO, 
  VIRTIO_GPU_RESP_OK_CAPSET, 
  VIRTIO_GPU_RESP_OK_EDID, 
  
  /* error responses */ 
  VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200, 
  VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY, 
  VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID, 
  VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID, 
  VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID, 
  VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER, 
} VirtioGpuCtrlType;

typedef struct PACKED {
  uint32_t type;
  uint32_t flags;
  uint64_t fence_id;
  uint32_t ctx_id;
  uint32_t padding;
} VirtioGpuCtrlHdr;

typedef struct PACKED {
  uint32_t x, y, w, h;
} VirtioGpuRect;

typedef struct PACKED {
  VirtioGpuCtrlHdr hdr;
  uint32_t resource_id;
  uint32_t format;
  uint32_t width;
  uint32_t height;
} VirtioGpuResourceCreate2D;

typedef struct PACKED {
  VirtioGpuCtrlHdr hdr;
  uint32_t resource_id;
  uint32_t nr_entries;
} VirtioGpuResourceAttachBacking;

typedef struct PACKED {
  VirtioGpuCtrlHdr hdr;
  VirtioGpuRect rect;
  uint32_t scanout_id;
  uint32_t resource_id;
} VirtioGpuSetScanout;

typedef struct PACKED {
  VirtioGpuCtrlHdr hdr;
  VirtioGpuRect rect;
  uint64_t offset;
  uint32_t resource_id;
  uint32_t padding;
} VirtioGpuTransferToHost2D;

typedef struct PACKED {
  VirtioGpuCtrlHdr hdr;
  VirtioGpuRect rect;
  uint32_t resource_id;
  uint32_t padding;
} VirtioGpuResourceFlush;

typedef struct PACKED {
  uint64_t addr;
  uint32_t length;
  uint32_t padding;
} VirtioGpuMemEntry;

// TODO: dynamic screen size
VirtioGpu virtio_gpu_init(VirtioDevice *dev) {
  ASSERT(dev->magic == VIRTIO_MAGIC);
  ASSERT(dev->version == 1);
  ASSERT(dev->device == VIRTIO_DEVICE_GPU);

  dev->status = 0;
  dev->status |= VIRTIO_STATUS_ACK;
  dev->status |= VIRTIO_STATUS_DRIVER;
  dev->guest_features = VIRTIO_NET_F_MAC;
  dev->status |= VIRTIO_STATUS_FEAT_OK;

  Virtq *vq = virtq_create(dev, 0);
  Virtq *cq = virtq_create(dev, 1);

  dev->status |= VIRTIO_STATUS_DRIVER_OK;

  uint8_t *buffer = (void *)alloc_pages(300);

  VirtioGpuResourceCreate2D req1 = {
    .hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    .resource_id = 1,
    .format = PIXEL_FORMAT,
    .width = DISPLAY_WIDTH,
    .height = DISPLAY_HEIGHT,
  };
  VirtioGpuCtrlHdr res1 = {0};

  VirtioGpuResourceAttachBacking req2 = {
    .hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    .resource_id = 1,
    .nr_entries = 1,
  };
  VirtioGpuMemEntry ent2 = {
    .addr = (uint32_t)buffer,
    .length = DISPLAY_WIDTH * DISPLAY_HEIGHT * 4,
  };
  VirtioGpuCtrlHdr res2 = {0};

  VirtioGpuSetScanout req3 = {
    .hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT,
    .rect.w = DISPLAY_WIDTH,
    .rect.h = DISPLAY_HEIGHT,
    .resource_id = 1,
    .scanout_id = 0,
  };
  VirtioGpuCtrlHdr res3 = {0};

  virtq_descf(vq, &req1, sizeof(req1), 0);
  virtq_descl(vq, &res1, sizeof(res1), 1);

  virtq_descf(vq, &req2, sizeof(req2), 0);
  virtq_descm(vq, &ent2, sizeof(ent2), 0);
  virtq_descl(vq, &res2, sizeof(res2), 1);

  virtq_descf(vq, &req3, sizeof(req3), 0);
  virtq_descl(vq, &res3, sizeof(res3), 1);

  __sync_synchronize();
  dev->queue_notify = 0;
  while (vq->avail.index != vq->used.index);
  vq->desc_index = 0;

  ASSERT(res1.type == VIRTIO_GPU_RESP_OK_NODATA);
  ASSERT(res2.type == VIRTIO_GPU_RESP_OK_NODATA);
  ASSERT(res3.type == VIRTIO_GPU_RESP_OK_NODATA);

  return (VirtioGpu){
    .dev = dev,
    .vq = vq,
    .cq = cq,
    .fb = (void *)buffer,
  };
}

void virtio_gpu_flush(VirtioGpu *gpu) {
  VirtioGpuTransferToHost2D req4 = {
    .hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    .rect.w = DISPLAY_WIDTH,
    .rect.h = DISPLAY_HEIGHT,
    .resource_id = 1,
  };
  VirtioGpuCtrlHdr res4 = {0};

  VirtioGpuResourceFlush req5 = {
    .hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    .rect.w = DISPLAY_WIDTH,
    .rect.h = DISPLAY_HEIGHT,
    .resource_id = 1,
  };
  VirtioGpuCtrlHdr res5 = {0};

  Virtq *vq = gpu->vq;
  virtq_descf(vq, &req4, sizeof(req4), 0);
  virtq_descl(vq, &res4, sizeof(res4), 1);

  virtq_descf(vq, &req5, sizeof(req5), 0);
  virtq_descl(vq, &res5, sizeof(res5), 1);

  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = 0;
  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = 2;

  __sync_synchronize();
  gpu->dev->queue_notify = 0;
  while (vq->avail.index != vq->used.index);
  vq->desc_index = 0;

  ASSERT(res4.type == VIRTIO_GPU_RESP_OK_NODATA);
  ASSERT(res5.type == VIRTIO_GPU_RESP_OK_NODATA);
}

void test_gpu() {
  VirtioGpu gpu = virtio_gpu_init((void *)0x10004000);

  for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
    gpu.fb[i] = bswap32(0xff000000);
  }

  virtio_gpu_flush(&gpu);

}
