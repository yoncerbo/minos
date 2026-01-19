#ifndef INCLUDE_INTERFACE_FS
#define INCLUDE_INTERFACE_FS

#include "common.h"

// for the forseeable future, I'm just gonna use
// enums, as I don't need the flexibility of function pointers
typedef enum {
  FS_NONE,
  FS_FAT32,
  FS_USTAR,
} FsType;

typedef struct {
  FsType type;
} Fs;

typedef enum {
  ENTRY_NONE,
  ENTRY_FILE,
  ENTRY_DIR,
} EntryType;

typedef struct {
  uint32_t size;
  uint32_t start;
  EntryType type;
} DirEntry;

typedef struct {
  uint16_t gen;
  uint16_t index;
} Fid;

typedef struct Vfs Vfs;

void vfs_mount(Vfs *vfs, Str path, Fs *fs);
Fid vfs_fopen(Vfs *vfs, Str path);
void vfs_fclose(Vfs *vfs, Fid fid);
void vfs_file_rw_sectors(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, uint8_t *buffer, bool is_write);
uint32_t vfs_fsize(Vfs *vfs, Fid fid);

// void fs_v1_mount_filesystem(Vfs *vfs, Str path, Fs *fs);
// Fid fs_v1_open_file(Vfs *vfs, Str path);
// void fs_v1_close_file(Vfs *vfs, Fid fid);
// void fs_v1_read_write_sectors(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, uint8_t *buffer, bool is_write);
// uint32_t fs_v1_get_file_size(Vfs *vfs, Fid fid);

#endif
