#include "common.h"

#define MAX_MOUNT_POINTS 256
#define MAX_FILE_SYSTEMS 256
#define MAX_FILES 256

typedef struct {
  uint16_t gen;
  uint16_t index;
} Fid;

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
  char name[32];
  uint32_t first_cluster;
  uint32_t size;
  uint16_t gen;
  VfsId fs;
} File;

typedef struct {
  char path[64];
  uint8_t len;
  VfsId fs;
} VfsMount;

typedef struct {
  VfsMount mounts[256]; // swap-back array
  VfsFs file_systems[256];
  File files[256];
  uint8_t mounts_len;
  uint8_t next_free_fs;
  uint8_t next_free_file;
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
  if (path.ptr[matching] == '/') matching++;
  return (_MatchResult){
    longest_match,
    (Str){ &path.ptr[matching], path.len - matching },
  };
}

Fid vfs_file_open(Vfs *vfs, Str path) {
  if (path.ptr[path.len - 1] == '/') path.len--;
  // TODO: first, check if the file is already open
  _MatchResult match = _vfs_mount_match(vfs, path);
  if (match.index == 256) return (Fid){0};

  // TODO: clean up this code
  VfsFs *fs = &vfs->file_systems[match.index];
  // For now only one file system type supported
  ASSERT(fs->type == VFS_FAT32);

  FatDriver *fat_driver = fs->data;
  uint32_t first_directory_cluster = fat_driver->root_cluster;

  Str subpath = match.subpath;
  Str name;
  while (1) {
    uint32_t len = 0;
    for (; len < subpath.len && match.subpath.ptr[len] != '/'; len++);
    name = (Str){ match.subpath.ptr, len };
    if (subpath.len <= len) break;
    // then change first_directory_cluster

    FatEntry *fat_entry = fat_find_directory_entry(
        fat_driver, first_directory_cluster, name);
    ASSERT(fat_entry->attr | FAT_DIRECTORY);
    first_directory_cluster = ((uint32_t)fat_entry->cluster_high << 16) | fat_entry->cluster_low;
  }

  FatEntry *fat_entry = fat_find_directory_entry(
      fat_driver, first_directory_cluster, name);
  ASSERT(fat_entry->attr ^ FAT_DIRECTORY);
  uint32_t cluster = ((uint32_t)fat_entry->cluster_high << 16) | fat_entry->cluster_low;

  uint32_t index = vfs->next_free_file++;
  ASSERT(index < 256);
  File *file = &vfs->files[index];
  // TODO: increment generation?
  file->first_cluster = cluster;
  file->size = fat_entry->size;
  // TODO: return fs id in the match
  file->fs = (VfsId){ match.index, vfs->file_systems[match.index].gen };
  ASSERT(name.len <= 32);
  memcpy(&file->name, name.ptr, name.len);

  return (Fid){ file->gen, index };
}

static inline uint32_t vfs_file_size(Vfs *vfs, Fid fid) {
  ASSERT(vfs->files[fid.index].gen == fid.gen);
  return vfs->files[fid.index].size;
}

void vfs_file_close(Vfs *vfs, Fid fid);

uint32_t vfs_file_read(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, char *buffer);
uint32_t vfs_file_write(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, const char *buffer);

uint32_t vfs_file_read_sectors(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, char *buffer) {
  ASSERT(vfs->files[fid.index].gen == fid.gen);
  File *file = &vfs->files[fid.index];
  ASSERT(vfs->file_systems[file->fs.index].gen == file->fs.gen);
  VfsFs *fs = &vfs->file_systems[file->fs.index];

  switch (fs->type) {
    case VFS_FAT32:
      FatDriver *driver = fs->data;
      uint32_t cluster = file->first_cluster;
      uint32_t start_in_clusters = start / driver->sectors_per_cluster;
      uint32_t end_in_clusters = (start + len) / driver->sectors_per_cluster +
        (start + len) % driver->sectors_per_cluster ? 1 : 0;

      DEBUGD(start_in_clusters);
      DEBUGD(end_in_clusters);

      uint32_t i = 0;
      for (; i < start_in_clusters; ++i) {
        cluster = fat_next_cluster(driver, cluster);
      }
      uint32_t sectors_read = 0;
      uint32_t sector = start % driver->sectors_per_cluster;
      for (; i < end_in_clusters; ++i) {
        uint32_t cluster_start_on_disk = first_sector_in_cluster(driver, cluster);
        for (; sector < driver->sectors_per_cluster && sectors_read < len; ++sector) {
          read_write_disk(driver->blkdev, buffer + SECTOR_SIZE * sectors_read,
              cluster_start_on_disk + sector, false);
          sectors_read++;
          LOG("reading sector %d\n", cluster_start_on_disk + sector);
        }
        sector = 0;
        cluster = fat_next_cluster(driver, cluster);
      }
      break;
    default:
      PANIC("unknown file system type: %d", fs->type);
  }

}

uint32_t vfs_file_write_sectors(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, const char *buffer);

// Ideas:
// different open modes
// close and remove
// stat
// resize
// change attributes
// write and resize
// vector write and read
// directory writing and reading, batch creating and removal, etc.

// TODO:
// what to do when 2 mounts have the same path
// add some path processing
// removing mounts
// changing mounts, getting mount data, etc.
// you can create two file systems from the same driver data,
//  might be a problem later
