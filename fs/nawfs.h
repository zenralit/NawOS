#ifndef NAWFS_H
#define NAWFS_H

#include <stddef.h>
#include <stdint.h>

#include "nawfs_format.h"

void fs_init();
void fs_list();
int fs_create(const char* name, const char* ext);
int fs_write(const char* name, const char* ext, const char* data);
const char* fs_read(const char* name, const char* ext);
int fs_delete(const char* name, const char* ext);
void fs_flush();
int fs_format();
int disk_write_sector(int lba, const uint8_t* buffer);
char* strstr(const char* haystack, const char* needle);
char* strncpy(char* dest, const char* src, unsigned int n);
void itoa(int value, char* str);
unsigned int strlen(const char* str);
int strcmp(const char* s1, const char* s2);
extern nawfs_entry entries[MAX_FILES];
extern int file_count;
int fs_read_to_buffer(const char* name, const char* ext, char* buffer, int max_len);

#endif
