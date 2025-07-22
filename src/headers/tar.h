#ifndef INCLUDE_TAR
#define INCLUDE_TAR

#include "common.h"
#include "virtio_blk.h"
#include "vfs.h"

typedef struct {
  Fs fs;
  VirtioBlkdev *blkdev;
  uint8_t buffer[512];
} TarDriver;

TarDriver tar_driver_init(VirtioBlkdev *blkdev);
DirEntry tar_find_file(TarDriver *driver, Str name);

#endif
