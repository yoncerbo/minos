#include "virtio.h"

Virtq *virtq_create(VirtioDevice *dev, uint32_t index) {
  Virtq *vq = (void *)alloc_pages(2);
  dev->queue_sel = index;
  dev->queue_num = VIRTQ_ENTRY_NUM;
  dev->queue_align = 0;
  dev->queue_pfn = (uint32_t)vq;
  return vq;
}
