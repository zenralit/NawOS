#include "nawfs.h"
#include "screen.h" 
#include "keyboard.h"
#include <string.h> 
#include "nawfs.h"
#include "disk.h"
#include "screen.h"
#include <stddef.h>

nawfs_entry entries[MAX_FILES];
char file_data[SECTOR_SIZE * (MAX_FILES + 1)];
int file_count = 0;
uint8_t fs_buffer[SECTOR_SIZE];
int memcmp(const void* s1, const void* s2, size_t n);

void fs_init() {
    asm volatile("cli");

    uint8_t sector_data[SECTOR_SIZE]; 

    for (int i = 1; i <= CATALOG_SECTORS; i++) {
        if (disk_read_sector(i, sector_data) != 0) {
            print("FS: failed to read catalog sector\n");
            asm volatile("sti");
            return;
        }

        for (int j = 0; j < SECTOR_SIZE / sizeof(nawfs_entry); j++) {
            nawfs_entry* e = (nawfs_entry*)(sector_data + j * sizeof(nawfs_entry));
            if (e->name[0] != 0 && file_count < MAX_FILES) {
                entries[file_count++] = *e;
            }
        }
    }
    asm volatile("sti");
}

void fs_flush() {
    uint8_t buffer[SECTOR_SIZE];

    for (int i = 0; i < CATALOG_SECTORS; i++) {
        for (int j = 0; j < SECTOR_SIZE / sizeof(nawfs_entry); j++) {
            int idx = i * (SECTOR_SIZE / sizeof(nawfs_entry)) + j;
            if (idx < file_count) {
                memcpy(buffer + j * sizeof(nawfs_entry), &entries[idx], sizeof(nawfs_entry));
            } else {
                for (int k = 0; k < sizeof(nawfs_entry); k++)
                    buffer[j * sizeof(nawfs_entry) + k] = 0;
            }
        }

        if (disk_write_sector(i + 1, buffer) != 0) {
            print("FS: failed to write catalog sector\n");
            return;
        }
    }
}

int find_file(const char* name, const char* ext) {
    for (int i = 0; i < file_count; i++) {
        if (strncmp(entries[i].name, name, FILENAME_LEN) == 0 &&
            strncmp(entries[i].ext, ext, EXTENSION_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

int fs_read_to_buffer(const char* name, const char* ext, char* buffer, int max_len) {
    const char* file_data = fs_read(name, ext);
    if (!file_data) return -1;
   
    int i;

    for (i = 0; i < max_len - 1 && file_data[i] != '\0'; i++) {
        buffer[i] = file_data[i];
    }
        buffer[i] = '\0';
    return i;
}

int fs_create(const char* name, const char* ext) {
    if (file_count >= MAX_FILES) return -1;
    if (find_file(name, ext) >= 0) return -1;
    nawfs_entry* e = &entries[file_count];
    memset(e, 0, sizeof(nawfs_entry));
    strncpy(e->name, name, FILENAME_LEN);
    strncpy(e->ext, ext, EXTENSION_LEN);
    e->sector = DATA_START_SECTOR + file_count;
    e->size = 0;
    file_count++;
    fs_flush();
    return 0;
}

int fs_write(const char* name, const char* ext, const char* data) {
    int i = find_file(name, ext);
    if (i < 0) return -1;
    size_t len = strlen(data);
    if (len > SECTOR_SIZE) len = SECTOR_SIZE;
    entries[i].size = len;
    uint8_t buffer[SECTOR_SIZE];
    memset(buffer, 0, SECTOR_SIZE);
    memcpy(buffer, data, len);

    if (disk_write_sector(entries[i].sector, buffer) != 0)
        return -1;

    entries[i].size = len;
    fs_flush();
    return 0;
}

const char* fs_read(const char* name, const char* ext) {
    for (int i = 0; i < file_count; ++i) {
                if (memcmp(entries[i].name, name, 8) == 0 && memcmp(entries[i].ext, ext, 3) == 0) {
                    static char buffer[512];
                        if (disk_read_sector(entries[i].sector, (uint8_t*)buffer) == 0) {
                            buffer[entries[i].size] = '\0'; 
                                return buffer;
                            } else {
                        return NULL;
                    }
                }
        }
    return NULL;
}


int fs_delete(const char* name, const char* ext) {
    int i = find_file(name, ext);
    if (i < 0) return -1;

    uint8_t zero_sector[SECTOR_SIZE] = {0};
    disk_write_sector(entries[i].sector, zero_sector);  

    for (int j = i; j < file_count - 1; j++) {
        entries[j] = entries[j + 1];
    }

    file_count--;
    fs_flush();
    return 0;
}

void fs_list() { /* TODO починить наконец то вывод*/
       for (int i = 0; i < file_count; i++) {
        print(" - ");
        print(entries[i].name);
        print(".");
        print(entries[i].ext);
        print("(");
        char size[8];
        itoa(entries[i].size, size);
        print(size);
        print(" bytes)\n");
        }
    
}

//  функции 



void* memset(void* dest, int val, size_t n) {
    unsigned char* d = dest;
    while (n--) *d++ = (unsigned char)val;
    return dest;
}


char* strncpy(char* dest, const char* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = 0;
    return dest;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

void itoa(int value, char* str) {
    char* p = str;
    char buf[10];
    int i = 0;
    if (value == 0) {
        *p++ = '0';
        *p = 0;
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) *p++ = buf[--i];
        *p = 0;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}