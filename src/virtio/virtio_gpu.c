#include "common.h"
#include "memory.h"
#include "virtio.h"

const uint32_t PIXEL_FORMAT = 67; // RGBA
const uint32_t DISPLAY_WIDTH = 640;
const uint32_t DISPLAY_HEIGHT = 480;

typedef struct {
  uint8_t r, g, b, a;
} Rgba;

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

typedef struct {
  VirtioDevice *dev;
  Virtq *vq;
  Virtq *cq;
} VirtioGpu;

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

  // 1) resource create 2d, attach backing, framebufer doesn't have to be contigious
  //    set scanout
  // 2) update framebuffer and scanout
  //  render, transfer to host, resource flush

  uint8_t *buffer = (void *)alloc_pages(310);

  uint32_t *fb = (void *)buffer;
  for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
    fb[i] = bswap32(0xffffff00);
  }

  VirtioGpuResourceCreate2D req1 = {
    .hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    .resource_id = 1,
    .format = PIXEL_FORMAT,
    .width = DISPLAY_WIDTH,
    .height = DISPLAY_HEIGHT,
  };
  VirtioGpuCtrlHdr res1;

  VirtioGpuResourceAttachBacking req2 = {
    .hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    .resource_id = 1,
    .nr_entries = 1,
  };
  VirtioGpuMemEntry ent2 = {
    .addr = (uint32_t)buffer,
    .length = DISPLAY_WIDTH * DISPLAY_HEIGHT * 4,
  };
  VirtioGpuCtrlHdr res2;

  VirtioGpuSetScanout req3 = {
    .hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT,
    .rect.w = DISPLAY_WIDTH,
    .rect.h = DISPLAY_HEIGHT,
    .resource_id = 1,
    .scanout_id = 0,
  };
  VirtioGpuCtrlHdr res3;

  vq->descs[0] = (VirtqDesc){
    .addr = (uint32_t)&req1,
    .len = sizeof(req1),
    .flags = VIRTQ_DESC_NEXT,
    .next = 1,
  };
  vq->descs[1] = (VirtqDesc){
    .addr = (uint32_t)&res1,
    .len = sizeof(res1),
    .flags = VIRTQ_DESC_WRITE,
  };

  vq->descs[2] = (VirtqDesc){
    .addr = (uint32_t)&req2,
    .len = sizeof(req2),
    .flags = VIRTQ_DESC_NEXT,
    .next = 3,
  };
  vq->descs[3] = (VirtqDesc){
    .addr = (uint32_t)&ent2,
    .len = sizeof(ent2),
    .flags = VIRTQ_DESC_NEXT,
    .next = 4,
  };
  vq->descs[4] = (VirtqDesc){
    .addr = (uint32_t)&res2,
    .len = sizeof(res2),
    .flags = VIRTQ_DESC_WRITE,
  };

  vq->descs[5] = (VirtqDesc){
    .addr = (uint32_t)&req3,
    .len = sizeof(req3),
    .flags = VIRTQ_DESC_NEXT,
    .next = 6,
  };
  vq->descs[6] = (VirtqDesc){
    .addr = (uint32_t)&res3,
    .len = sizeof(res3),
    .flags = VIRTQ_DESC_WRITE,
  };

  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = 0;
  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = 2;
  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = 5;

  __sync_synchronize();
  dev->queue_notify = 0;

  while (vq->avail.index != vq->used.index) {
    printf("Waiting\n");
  }

  DEBUGX(res1.type);
  DEBUGX(res2.type);
  DEBUGX(res3.type);

  VirtioGpuTransferToHost2D req4 = {
    .hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    .rect.w = DISPLAY_WIDTH,
    .rect.h = DISPLAY_HEIGHT,
    .resource_id = 1,
  };
  VirtioGpuCtrlHdr res4;

  VirtioGpuResourceFlush req5 = {
    .hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    .rect.w = DISPLAY_WIDTH,
    .rect.h = DISPLAY_HEIGHT,
    .resource_id = 1,
  };
  VirtioGpuCtrlHdr res5;

  vq->descs[0] = (VirtqDesc){
    .addr = (uint32_t)&req4,
    .len = sizeof(req4),
    .flags = VIRTQ_DESC_NEXT,
    .next = 1,
  };
  vq->descs[1] = (VirtqDesc){
    .addr = (uint32_t)&res4,
    .len = sizeof(res4),
    .flags = VIRTQ_DESC_WRITE,
  };

  vq->descs[2] = (VirtqDesc){
    .addr = (uint32_t)&req5,
    .len = sizeof(req5),
    .flags = VIRTQ_DESC_NEXT,
    .next = 3,
  };
  vq->descs[3] = (VirtqDesc){
    .addr = (uint32_t)&res5,
    .len = sizeof(res5),
    .flags = VIRTQ_DESC_WRITE,
  };

  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = 0;
  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = 2;

  __sync_synchronize();
  dev->queue_notify = 0;

  while (vq->avail.index != vq->used.index) {
    printf("Waiting\n");
  }

  DEBUGX(res4.type);
  DEBUGX(res5.type);

  return (VirtioGpu){
    .dev = dev,
    .vq = vq,
    .cq = cq,
  };
}

void test_gpu() {
  VirtioGpu gpu = virtio_gpu_init((void *)0x10004000);
}
