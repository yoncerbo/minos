#ifndef INCLUDE_VFS
#define INCLUDE_VFS

#include "common.h"

typedef struct {
  uint16_t gen;
  uint16_t index;
} Fid;

typedef struct Vfs Vfs;

void vfs_mount(Vfs *vfs, Str path, Fs *fs);
Fid vfs_fopen(Vfs *vfs, Str path);
void vfs_fclose(Vfs *vfs, Fid fid);
void vfs_file_rw_sectors(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, uint8_t *buffer, bool is_write);
inline uint32_t vfs_fsize(Vfs *vfs, Fid fid);

#endif
