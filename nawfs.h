#ifndef NAWFS_H
#define NAWFS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_FILES 64
#define FILENAME_LEN 8
#define EXTENSION_LEN 3
#define MAX_FILE_SIZE 512
#define SECTOR_SIZE 512
#define DATA_START_SECTOR 10  
#define CATALOG_SECTORS 9

typedef struct {
    char name[8];
    char ext[3];
    uint16_t size;
    uint16_t sector;
    char reserved[17];
} __attribute__((packed)) nawfs_entry;

void fs_init();
void fs_list();
int fs_create(const char* name, const char* ext);
int fs_write(const char* name, const char* ext, const char* data);
const char* fs_read(const char* name, const char* ext);
int fs_delete(const char* name, const char* ext);
void fs_flush();
int disk_write_sector(int lba, const uint8_t* buffer);
char* strstr(const char* haystack, const char* needle);
char* strncpy(char* dest, const char* src, unsigned int n);
void itoa(int value, char* str);
unsigned int strlen(const char* str);
int strcmp(const char* s1, const char* s2);
extern nawfs_entry entries[MAX_FILES];
extern int file_count;

#endif