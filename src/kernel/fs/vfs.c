#include "vfs.h"

#define MAX_MOUNT_POINTS 256
#define MAX_FILES 256

typedef struct {
  char name[32];
  Fs* fs; // if zero, then entry is free
  uint32_t start; // used as next when free
  uint32_t size;
  uint16_t gen;
} File;

typedef struct {
  char path[64];
  uint8_t len;
  Fs *fs;
} VfsMount;

struct Vfs {
  VfsMount mounts[256]; // swap-back array
  File files[256];
  uint8_t mounts_len;
  uint8_t next_free_file;
};

// TODO: add some basic path processing
void vfs_mount(Vfs *vfs, Str path, Fs *fs) {
  // TODO: add some path processing
  ASSERT(vfs->mounts_len < MAX_MOUNT_POINTS);
  ASSERT(path.len <= 63); // leave one space for 0

  VfsMount *mount = &vfs->mounts[vfs->mounts_len++];
  mount->len = path.len;
  mount->fs = fs;
  memcpy(&mount->path, path.ptr, path.len);
}

typedef struct {
  Str subpath;
  Fs *fs;
} MatchResult;

MatchResult vfs_mount_match(Vfs *vfs, Str path) {
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
  return (MatchResult){
    .subpath = (Str){ &path.ptr[matching], path.len - matching },
    .fs = longest_match == MAX_MOUNT_POINTS ? 0 : vfs->mounts[longest_match].fs,
  };
}

Fid vfs_fopen(Vfs *vfs, Str path) {
  if (path.ptr[path.len - 1] == '/') path.len--;
  // TODO: first, check if the file is already open
  MatchResult match = vfs_mount_match(vfs, path);
  ASSERT(match.fs != 0);

  // TODO: clean up this code
  switch (match.fs->type) {
    case FS_FAT32: {
      FatDriver *fat_driver = (void *)match.fs;
      uint32_t first_directory_cluster = fat_driver->root_cluster;
      DirEntry entry;

      Str subpath = match.subpath;
      Str name;
      while (1) {
        uint32_t len = 0;
        for (; len < subpath.len && subpath.ptr[len] != '/'; len++);
        name = (Str){ subpath.ptr, len };
        if (subpath.len <= len) break;
        subpath.len -= len + 1; // include '/'
        subpath.ptr += len + 1;

        entry = fat_find_directory_entry(fat_driver, first_directory_cluster, name);
        ASSERT(entry.type == ENTRY_DIR);
        first_directory_cluster = entry.start;
      }

      entry = fat_find_directory_entry(fat_driver, first_directory_cluster, name);
      ASSERT(entry.type == ENTRY_FILE);

      // TODO: reuse closed files
      uint32_t index = vfs->next_free_file++;
      ASSERT(index < 256);
      File *file = &vfs->files[index];
      // TODO: increment generation?
      file->start = entry.start;
      file->size = entry.size;
      file->fs = match.fs;
      ASSERT(name.len <= 32);
      memcpy(&file->name, name.ptr, name.len);

      return (Fid){ file->gen, index };
    } break;
    case FS_USTAR: {
      TarDriver *driver = (void *)match.fs;
      DirEntry entry = tar_find_file(driver, match.subpath);
      Str name = match.subpath;
      ASSERT(entry.type == ENTRY_FILE);

      // TODO: reuse closed files
      uint32_t index = vfs->next_free_file++;
      ASSERT(index < 256);
      File *file = &vfs->files[index];
      // TODO: increment generation?
      file->start = entry.start;
      file->size = entry.size;
      file->fs = match.fs;
      ASSERT(name.len <= 32);
      memcpy(&file->name, name.ptr, name.len);

      return (Fid){ file->gen, index };
    } break;
    default:
      PANIC("Unsupported file system type: %d\n", match.fs->type);
  }
}

void vfs_fclose(Vfs *vfs, Fid fid) {
  ASSERT(vfs->files[fid.index].gen == fid.gen);
  File *file = &vfs->files[fid.index];
  file->fs = 0; // mark entry as free
  file->start = vfs->next_free_file;
  vfs->next_free_file = fid.index;
}

extern inline uint32_t vfs_fsize(Vfs *vfs, Fid fid) {
  ASSERT(vfs->files[fid.index].gen == fid.gen);
  return vfs->files[fid.index].size;
}

void vfs_file_rw_sectors(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, uint8_t *buffer, bool is_write) {
  ASSERT(vfs->files[fid.index].gen == fid.gen);
  File *file = &vfs->files[fid.index];

  switch (file->fs->type) {
    case FS_FAT32: {
      uint32_t cluster = file->start;
      fat_rw_sectors((void *)file->fs, cluster, start, len, buffer, is_write);
    } break;
    case FS_USTAR: {
      TarDriver *driver = (void *)file->fs;
      virtio_blk_rw(driver->blkdev, buffer, file->start + start, len, is_write);
    } break;
    default:
      PANIC("unknown file system type: %d", file->fs->type);
  }

}

// uint32_t vfs_file_read(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, char *buffer);
// uint32_t vfs_file_write(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, const char *buffer);
// uint32_t vfs_file_read_sectors(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, char *buffer);
// uint32_t vfs_file_write_sectors(Vfs *vfs, Fid fid, uint32_t start, uint32_t len, const char *buffer);

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
