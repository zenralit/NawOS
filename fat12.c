#include "keyboard.h"
#include "screen.h"
#include "ports.h"
#include "fat12.h"  
#include <string.h>

#define SECTOR_SIZE 512
#define FLOPPY_DISK 0x00
#define ROOT_DIR_SECTOR 19
#define SECTOR_SIZE 512
#define MAX_ROOT_ENTRIES 224

static uint8_t sector_buffer[SECTOR_SIZE];

typedef struct {
    char filename[8];
    char ext[3];
    uint8_t attr;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t start_cluster;
    uint32_t size;
} __attribute__((packed)) DirEntry;

int read_sector(uint8_t* buffer, uint32_t lba) {
    uint8_t chs[] = {
        0x00, // Drive number (A:)
        0x01, // Head
        0x01, // Sector
        0x00, // Cylinder
        0x01, // Count
        0x00  // Buffer segment offset
    };
    
    return 0; // success
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--) *p++ = (unsigned char)c;
        return s;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
        return len;
}

void list_root_dir() {
    if (read_sector(sector_buffer, 19) != 0) {
        print("Error reading root directory\n");
        return;
    }

    for (int i = 0; i < SECTOR_SIZE; i += 32) {
        char* filename = (char*)&sector_buffer[i];
        if (filename[0] == 0x00) break;
        if ((uint8_t)filename[0] == 0xE5) continue;

        for (int j = 0; j < 11; j++) {
            put_char(filename[j]);
        }
        print("\n");
    }
}

int write_sector(const uint8_t* buffer, uint32_t lba) {
    outb(0x1F6, 0xE0);                        
    outb(0x1F2, 1);                           
    outb(0x1F3, lba & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); 
    outb(0x1F7, 0x30);                        

    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        outw(0x1F0, ((uint16_t*)buffer)[i]);
    }

    return 0;
}

int create_file(const char* filename) {
    uint8_t sector[SECTOR_SIZE];
    read_sector(sector, ROOT_DIR_SECTOR);
    DirEntry* entries = (DirEntry*)sector;

    for (int i = 0; i < MAX_ROOT_ENTRIES; i++) {
        if (entries[i].filename[0] == 0x00 || entries[i].filename[0] == 0xE5) {
            memset(&entries[i], 0, sizeof(DirEntry));            
            memset(entries[i].filename, ' ', 11);
            int len = strlen(filename);
            int dot = -1;
            for (int j = 0; j < len; j++) {
                if (filename[j] == '.') {
                    dot = j;
                    break;
                }
                if (j < 8) entries[i].filename[j] = filename[j];
            }

            if (dot != -1) {
                for (int j = dot + 1, k = 0; j < len && k < 3; j++, k++) {
                    entries[i].filename[8 + k] = filename[j];
                }
            }

            entries[i].attr = 0x20;  
            entries[i].start_cluster = 0;
            entries[i].size = 0;

            write_sector((uint8_t*)entries, ROOT_DIR_SECTOR);

            print("File created.\n");
            return 0;
        }
    }

    print("Root directory full.\n");
    return -1;
}