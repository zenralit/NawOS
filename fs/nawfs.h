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
extern nawfs_entry entries[MAX_FILES];
extern int file_count;
int fs_read_to_buffer(const char* name, const char* ext, char* buffer, int max_len);

#endif
