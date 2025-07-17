#include "common.h"

// https://wiki.osdev.org/FAT

typedef struct {
  uint8_t bootjmp[3];
  uint8_t oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sector_count;
  uint8_t table_count;
  uint16_t root_entry_count;
  uint16_t total_sectors_16;
  uint8_t media_type;
  uint16_t table_size_16;
  uint16_t sectors_per_track;
  uint16_t head_side_count;
  uint32_t hidden_sector_count;
  uint32_t total_sectors_32;
  uint8_t extended_section[54];
} __attribute__((packed)) fat_BS_t;

typedef struct {
  uint32_t table_size_32;
  uint16_t extended_flags;
  uint16_t fat_version;
  uint32_t root_cluster;
  uint16_t fat_info;
  uint16_t backup_BS_sector;
  uint8_t reserved_0[12];
  uint8_t drive_number;
  uint8_t reserved_1;
  uint8_t boot_signature;
  uint32_t volume_id;
  uint8_t volume_label[11]; 
  uint8_t fat_type_label[8];
} __attribute__((packed)) fat_extBS_32;

typedef enum {
  FAT_READ_ONLY = 0x01,
  FAT_HIDDEN = 0x02,
  FAT_SYSTEM = 0x04,
  FAT_VOLUME_ID = 0x08,
  FAT_DIRECTORY = 0x10,
  FAT_ARCHIVE = 0x20,
  FAT_LFN = 0x0f,
} FatAttr;

typedef struct {
  uint8_t name[8];
  uint8_t ext[3];
  uint8_t attr;
  uint8_t _reserved;
  // creation time in hundredths of seconds or thenths,
  // doesn't seem clear
  uint8_t _reserved2;
  // hour 5 bits, minutes 6, seconds 5
  uint16_t ctime;
  // year 7 bits, month 4, day 5
  uint16_t cdate;
  uint16_t adate;
  uint16_t cluster_high;
  uint16_t mtime;
  uint16_t mdate;
  uint16_t cluster_low;
  uint32_t size;
} __attribute__((packed)) FatEntry;

typedef struct {
  uint8_t order;
  uint16_t name_1[5];
  uint8_t attr;
  uint8_t type;
  uint8_t checksum;
  uint16_t name_2[6];
  uint8_t _reserved[2];
  uint16_t name_3[2];
} __attribute__((packed)) FatLfn;

typedef struct {
  Fs fs; // every file system driver needs this header
  uint8_t *buffer;
  VirtioBlkdev *blkdev;
  uint32_t first_data_sector;
  uint32_t first_fat_sector;
  uint32_t root_cluster;
  uint8_t sectors_per_cluster;
} FatDriver;

static inline int first_sector_in_cluster(const FatDriver *driver, uint32_t cluster) {
  return ((cluster - 2) * driver->sectors_per_cluster) + driver->first_data_sector;
}

typedef enum {
  FAT_ENTRY_FREE = 0,
  // any other is the next cluster in chain
  FAT_ENTRY_BAD = 0x0ffffff7,
  FAT_ENTRY_END = 0x0ffffff8, // if >= then end of chain link
} FatTableEntry;

// note: uses buffer in dat to read fat table from disk
FatTableEntry fat_next_cluster(FatDriver *driver, uint32_t cluster) {
  uint32_t fat_offset = cluster * 4;
  uint32_t fat_sector = driver->first_fat_sector + (fat_offset / SECTOR_SIZE);
  uint32_t ent_offset = fat_offset % SECTOR_SIZE;

  // TODO: cache it instead of reading from disk for every lookup
  read_write_disk(driver->blkdev, driver->buffer, fat_sector, false);

  uint32_t table_value = *(uint32_t *)&driver->buffer[ent_offset];
  return table_value & 0x0fffffff; // the highes 4 bits are reserved
}

// returns pointer to the internal buffer, that will be overwritten
FatEntry *fat_find_directory_entry(FatDriver *driver, uint32_t first_directory_cluster, Str name) {
  FatEntry *entries = (void *)driver->buffer;
  uint32_t sector = first_sector_in_cluster(driver, first_directory_cluster);

  // TODO: do it for every sector in cluster
  read_write_disk(driver->blkdev, driver->buffer, sector, false);

  bool last_lfn_matched = false;

  for (uint32_t i = 0; i < 16; ++i) {
    FatEntry *entry = &entries[i];
    if (!entry->name[0]) break;
    if (entry->name[0] == 0xe5) continue; // unused
    // TODO: possbily multiple lfn entries, maybe across clusters
    if (entry->attr == FAT_LFN) {
      FatLfn *lfn = (void *)entry;
      // ASSERT(name.len <= 13);
      char buffer[14];
      buffer[0] = lfn->name_1[0];
      buffer[1] = lfn->name_1[1];
      buffer[2] = lfn->name_1[2];
      buffer[3] = lfn->name_1[3];
      buffer[4] = lfn->name_1[4];
      buffer[5] = lfn->name_2[0];
      buffer[6] = lfn->name_2[1];
      buffer[7] = lfn->name_2[2];
      buffer[8] = lfn->name_2[3];
      buffer[9] = lfn->name_2[4];
      buffer[10] = lfn->name_2[5];
      buffer[11] = lfn->name_3[0];
      buffer[12] = lfn->name_3[1];
      buffer[14] = 0;
      uint32_t offset = ((lfn->order & 0xf) - 1) * 13;
      uint32_t order = lfn->order & 0xf;
      uint32_t k = 0;
      for (; k + offset < name.len && buffer[k] == name.ptr[k + offset]; ++k);
      last_lfn_matched = k == name.len || k == 13;
      continue;
    }
    if (last_lfn_matched) return entry;
    // TODO: maybe check if the name in entry matches

    // uint32_t cluster = ((uint32_t)entry.cluster_high << 16) | entry.cluster_low;
    // uint32_t sector = first_sector_in_cluster(&driver, cluster);
    // DEBUGD(cluster);
    // DEBUGD(sector);
  }
  PANIC("No file found!");

  // TODO: get next cluster, if necessary
}

FatDriver fat_driver_init(VirtioBlkdev *blkdev) {
  FatDriver driver = {
    .fs.type = FS_FAT32,
    .blkdev = blkdev,
    .buffer = (void *)alloc_pages(1)
  };
  read_write_disk(blkdev, driver.buffer, 0, false);

  fat_BS_t *bs = (void *)driver.buffer;
  fat_extBS_32 *ebs = (void *)&bs->extended_section;

  // TODO: move it behind a debug flag or something
  LOG("Reading FAT disk boot record data\n");
  DEBUGD(bs->bytes_per_sector);
  DEBUGD(bs->sectors_per_cluster);
  DEBUGD(bs->reserved_sector_count);
  DEBUGD(bs->table_count);
  DEBUGD(bs->root_entry_count);
  DEBUGD(bs->total_sectors_16);
  DEBUGD(bs->table_size_16);
  DEBUGD(bs->sectors_per_track);
  DEBUGD(bs->head_side_count);
  DEBUGD(bs->hidden_sector_count);
  DEBUGD(bs->total_sectors_32);

  ASSERT(bs->bytes_per_sector == SECTOR_SIZE);

  uint32_t total_sectors = bs->total_sectors_16 ? bs->total_sectors_16 : bs->total_sectors_32;
  uint32_t fat_size = bs->table_size_16 ? bs->table_size_16 : ebs->table_size_32;
  uint32_t first_data_sector = bs->reserved_sector_count + (bs->table_count * fat_size);
  uint32_t data_sectors = total_sectors - (bs->reserved_sector_count + (bs->table_count * fat_size));
  uint32_t total_clusters = data_sectors / bs->sectors_per_cluster;

  uint32_t first_fat_sector = bs->reserved_sector_count;
  uint32_t root_cluster = ebs->root_cluster;

  DEBUGD(total_sectors);
  DEBUGD(fat_size);
  DEBUGD(data_sectors);
  DEBUGD(total_clusters);
  DEBUGD(first_data_sector);
  DEBUGD(first_fat_sector);
  DEBUGD(data_sectors);
  DEBUGD(total_clusters);
  DEBUGD(root_cluster);

  // should be fat 32
  ASSERT(total_clusters > 65525);

  driver.sectors_per_cluster = bs->sectors_per_cluster;
  driver.first_data_sector = first_data_sector;
  driver.first_fat_sector = bs->reserved_sector_count;
  driver.root_cluster = ebs->root_cluster;

  return driver;
}
