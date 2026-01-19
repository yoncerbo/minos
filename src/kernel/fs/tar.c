#include "vfs.h"

// https://wiki.osdev.org/USTAR

typedef struct PACKED {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char _padding[12];
} TarHeader;

uint32_t oct_to_bin(char *str, uint32_t size) {
  uint32_t number = 0;
  while (size-- > 0) {
    number *= 8;
    number += *str - '0';
    str++;
  }
  return number;
}

TarDriver tar_driver_init(VirtioBlkdev *blkdev) {
  TarDriver driver = {
    .fs.type = FS_USTAR,
    .blkdev = blkdev,
  };

  return driver;
}

DirEntry tar_find_file(TarDriver *driver, Str name) {
  ASSERT(name.len <= 100);
  uint32_t sector = 0, size;
  TarHeader *header;

  while (sector < driver->blkdev->sector_capacity) {
    virtio_blk_rw(driver->blkdev, driver->buffer, sector, 1, false);
    header = (void *)driver->buffer;
    size = oct_to_bin(header->size, sizeof(header->size) - 1);

    if (!strncmp(header->name, name.ptr, name.len)) {
      ASSERT(header->type == '0'); // normal file
      return (DirEntry){
        .type = ENTRY_FILE,
        .size = size,
        .start = sector + 1,
      };
    }
    sector += 1 + size / SECTOR_SIZE + (size % SECTOR_SIZE ? 1 : 0);
  }
  return (DirEntry){0};
}
