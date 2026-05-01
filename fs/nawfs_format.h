#ifndef NAWFS_FORMAT_H
#define NAWFS_FORMAT_H

#include <stdint.h>

#define MAX_FILES 64
#define FILENAME_LEN 8
#define EXTENSION_LEN 3
#define SECTOR_SIZE 512
#define MAX_FILE_SIZE SECTOR_SIZE
#define KERNEL_START_SECTOR 1
#define KERNEL_RESERVED_SECTORS 128
#define FS_SUPERBLOCK_SECTOR (KERNEL_START_SECTOR + KERNEL_RESERVED_SECTORS)
#define CATALOG_START_SECTOR (FS_SUPERBLOCK_SECTOR + 1)
#define CATALOG_SECTORS 9
#define DATA_START_SECTOR (CATALOG_START_SECTOR + CATALOG_SECTORS)
#define DATA_SECTOR_COUNT MAX_FILES
#define NAWFS_MAGIC_LEN 8
#define NAWFS_VERSION 1

typedef struct {
    char name[FILENAME_LEN];
    char ext[EXTENSION_LEN];
    uint16_t size;
    uint16_t sector;
    char reserved[17];
} __attribute__((packed)) nawfs_entry;

typedef struct {
    char magic[NAWFS_MAGIC_LEN];
    uint16_t version;
    uint16_t max_files;
    uint16_t catalog_start_sector;
    uint16_t catalog_sectors;
    uint16_t data_start_sector;
    uint16_t data_sector_count;
    char reserved[492];
} __attribute__((packed)) nawfs_superblock;

#endif
