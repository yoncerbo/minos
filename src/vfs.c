#include "common.h"

#define MAX_MOUNT_POINTS 256
#define MAX_FILE_SYSTEMS 256

typedef enum {
  VFS_NONE,
  VFS_FAT32,
} VfsFsType;

typedef struct {
  void *data;
  VfsFsType type;
  uint8_t gen;
} VfsFs;

typedef struct {
  uint8_t index;
  uint8_t gen;
} VfsId;

typedef struct {
  char path[64];
  uint8_t len;
  VfsId fs;
} VfsMount;

typedef struct {
  VfsMount mounts[256]; // swap-back array
  VfsFs file_systems[256];
  uint8_t mounts_len;
  uint8_t next_free_fs;
} Vfs;

VfsId vfs_fs_add(Vfs *vfs, VfsFsType type, void *data) {
  ASSERT(vfs->next_free_fs < MAX_FILE_SYSTEMS);
  uint8_t index = vfs->next_free_fs++;
  VfsFs *fs = &vfs->file_systems[index];
  fs->type = type;
  fs->data = data;

  return (VfsId){ index, fs->gen };
}

void vfs_mount_add(Vfs *vfs, Str path, VfsId fs) {
  ASSERT(vfs->mounts_len < MAX_MOUNT_POINTS);
  ASSERT(path.len <= 63); // leave one space for 0
  ASSERT(vfs->file_systems[fs.index].gen == fs.gen);

  VfsMount *mount = &vfs->mounts[vfs->mounts_len++];
  mount->len = path.len;
  mount->fs = fs;
  memcpy(&mount->path, path.ptr, path.len);
}

typedef struct {
  uint32_t index;
  Str subpath;
} _MatchResult;

_MatchResult _vfs_mount_match(Vfs *vfs, Str path) {
  uint32_t longest_match = MAX_MOUNT_POINTS;
  uint32_t matching = 0;
  for (uint32_t i = 0; i <  vfs->mounts_len; ++i) {
    VfsMount *mount = &vfs->mounts[i];
    if (mount->len > path.len) continue;
    uint32_t j = 0;
    for (; j < mount->len; ++j) {
      if (mount->path[j] == path.ptr[j]) continue;
      j = 0;
      break;
    }
    if (j <= matching) continue;
    matching = j;
    longest_match = i;
  }
  return (_MatchResult){
    longest_match,
    (Str){ &path.ptr[matching], path.len - matching },
  };
}


// TODO: 
// what to do when 2 mounts have the same path
// add some path processing
// removing mounts
// changing mounts, getting mount data, etc.
// you can create two file systems from the same driver data,
//  might be a problem later
