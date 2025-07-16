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
} fat_BS_t;

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
} fat_extBS_32;

typedef struct {
  fat_BS_t *bs;
  fat_extBS_32 *ebs;
  uint32_t first_data_sector;
} FatData;

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
  uint8_t _reserved;
  uint8_t sec;
  // creation time in hundredths of seconds or thenths,
  // doesn't seem clear
  uint8_t _reserved2;
  uint16_t ctime;
  uint16_t cdate;
  uint16_t adate;
  uint16_t cluster_high;
  uint16_t mtime;
  uint16_t mdate;
  uint16_t cluster_low;
  uint32_t size;
} FatEntry;

typedef struct {
  uint8_t order;
  uint16_t name_1[5];
  uint8_t attr;
  uint8_t type;
  uint8_t checksum;
  uint16_t name_2[6];
  uint8_t _reserved[2];
  uint16_t name_3[2];
} FatLfn;

static inline int first_cluster_sector(FatData data, int cluster) {
  return ((cluster - 2) * data.bs->sectors_per_cluster) + data.first_data_sector;
}

#define DEBUGD(var) \
  printf(STRINGIFY(var) "=%d\n", var)

void test_fat(VirtioBlkdev blkdev) {
  char buf[SECTOR_SIZE];
  read_write_disk(blkdev, buf, 0, false);

  fat_BS_t *bs = (void *)buf;
  fat_extBS_32 *ebs = (void *)&bs->extended_section;

  printf("oem_name: %s\n", bs->oem_name);
  printf("bytes per sector: %d\n", bs->bytes_per_sector);

  uint32_t total_sectors = bs->total_sectors_16 ? bs->total_sectors_16 : bs->total_sectors_32;
  uint32_t fat_size = bs->table_size_16 ? bs->table_size_16 : ebs->table_size_32;

  DEBUGD(total_sectors);
  DEBUGD(fat_size);

  uint32_t data_sectors = total_sectors - (bs->reserved_sector_count + (bs->table_count * fat_size));
  uint32_t total_clusters = data_sectors / bs->sectors_per_cluster;
  uint32_t first_data_sector = bs->reserved_sector_count + (bs->table_count * fat_size);

  DEBUGD(data_sectors);
  DEBUGD(total_clusters);
  DEBUGD(first_data_sector);

  // should be fat 32
  // ASSERT(total_clusters > 65525); 

  FatData data = {
    .bs = bs,
    .ebs = ebs,
    .first_data_sector = first_data_sector,
  };

  // TODO: get the root directory properly
  uint32_t root_cluster = 2;
  // uint32_t root_sector = first_cluster_sector(data, root_cluster);
  uint32_t root_sector = 5;
  DEBUGD(root_cluster);
  DEBUGD(root_sector);

  // TODO: how to get sector size?
  char buf2[SECTOR_SIZE];
  read_write_disk(blkdev, buf2, root_sector, false);

  uint8_t *entries = buf2;

  uint32_t count = 0;
  for (uint32_t i = 0; i < SECTOR_SIZE; i += 32) {
    uint8_t *entry = &entries[i];
    if (!*entry) break;
    if (*entry == 0xe5) {
      printf("Unused entry\n");
      continue;
    }
    if (entry[11] == 0x0f) {
      printf("long name entry\n");
      continue;
    }
    count++;
    printf("name: %s\n", entry);
  }
  LOG("Discovered %d entries in root directory\n", count);
}
