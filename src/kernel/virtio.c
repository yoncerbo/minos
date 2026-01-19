#include "common.h"
#include "virtio.h"

Virtq *virtq_create(VirtioDevice *dev, uint32_t index) {
  Virtq *vq = (void *)alloc_pages(2);
  dev->queue_sel = index;
  dev->queue_num = VIRTQ_ENTRY_NUM;
  dev->queue_align = 0;
  dev->queue_pfn = (uint32_t)vq;
  return vq;
}

extern inline void virtq_descf(Virtq *vq, void *addr, uint16_t len, bool is_write) {
  vq->avail.ring[vq->avail.index++ % VIRTQ_ENTRY_NUM] = vq->desc_index;
  vq->descs[vq->desc_index++] = (VirtqDesc){
    .addr = (uint32_t)addr,
    .len = len,
    .flags = (is_write << 1) | VIRTQ_DESC_NEXT,
    .next = vq->desc_index,
  };
}

extern inline void virtq_descm(Virtq *vq, void *addr, uint16_t len, bool is_write) {
  vq->descs[vq->desc_index++] = (VirtqDesc){
    .addr = (uint32_t)addr,
    .len = len,
    .flags = (is_write << 1) | VIRTQ_DESC_NEXT,
    .next = vq->desc_index,
  };
}

extern inline void virtq_descl(Virtq *vq, void *addr, uint16_t len, bool is_write) {
  vq->descs[vq->desc_index++] = (VirtqDesc){
    .addr = (uint32_t)addr,
    .len = len,
    .flags = is_write << 1,
    .next = 0,
  };
}
