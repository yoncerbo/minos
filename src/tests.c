#include "common.h"
#include "hardware.h"
#include "sbi.h"
#include "uart.c"
#include "print.c"
#include "memory.c"
#include "vfs.h"
#include "virtio/virtio.c"
#include "virtio/virtio_blk.c"
#include "virtio/virtio_net.c"
#include "fs/tar.c"
#include "fs/fat.c"
#include "fs/vfs.c"
#include "networking.c"

ALIGNED(4) __attribute__((interrupt("supervisor")))
void handle_supervisor_interrupt() {
  PANIC("An interrupt occured!");
}

void kernel_main(void) {
  memset(BSS_START, 0, BSS_END - BSS_START);
  LOG("Hello from testing!\n");

  LOG("Test: integer sizes\n");
  ASSERT(sizeof(uint64_t) == 8);
  ASSERT(sizeof(int64_t) == 8);
  ASSERT(sizeof(uint32_t) == 4);
  ASSERT(sizeof(int32_t) == 4);
  ASSERT(sizeof(uint16_t) == 2);
  ASSERT(sizeof(int16_t) == 2);
  ASSERT(sizeof(uint8_t) == 1);
  ASSERT(sizeof(int8_t) == 1);

  // TODO: test memory allocation

  putchar(10);
  LOG("Test: file systems\n");

  LOG("Setting up FAT driver\n");
  VirtioBlkdev blkdev = virtio_blk_init((void *)0x10001000);
  FatDriver fat_driver = fat_driver_init(&blkdev);

  LOG("Setting up USTAR driver\n");
  VirtioBlkdev tar_blkdev = virtio_blk_init((void *)0x10002000);
  TarDriver tar_driver = tar_driver_init(&tar_blkdev);

  LOG("Mounting files systems\n");
  Vfs vfs;
  vfs_mount(&vfs, STR("/fat/"), &fat_driver.fs);
  vfs_mount(&vfs, STR("/tar/"), &tar_driver.fs);

  LOG("Testing file systems \n");
  uint8_t buffer[SECTOR_SIZE];
  {
    Fid file = vfs_fopen(&vfs, STR("/tar/big_file.txt"));
    ASSERT(vfs_fsize(&vfs, file) == 726);
    vfs_file_rw_sectors(&vfs, file, 0, 1, buffer, false);
    const char str[] = "some big file content right here";
    ASSERT(strncmp(buffer, str, CSTR_LEN(str)));
  }
  {
    Fid file = vfs_fopen(&vfs, STR("/fat/dir/some_long_filename.txt"));
    vfs_file_rw_sectors(&vfs, file, 1, 1, buffer, false);
    const char str[] = "ne sector\nmore content than o";
    ASSERT(strncmp(buffer, str, CSTR_LEN(str)));
  }

  putchar(10);
  LOG("Test: networking - DHCP request\n");
  VirtioNetdev netdev = virtio_net_init(VIRTIO_NET);
  net_dhcp_request(&netdev);

  sbi_shutdown();
}
