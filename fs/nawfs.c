#include "nawfs.h"
#include "drivers/disk/disk.h"
#include "drivers/screen/screen.h"
#include <stddef.h>

nawfs_entry entries[MAX_FILES];
int file_count = 0;

void* memset(void* dest, int val, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
char* strncpy(char* dest, const char* src, unsigned int n);
unsigned int strlen(const char* s);
void itoa(int value, char* str);

static const char nawfs_magic[NAWFS_MAGIC_LEN] = {'N', 'A', 'W', 'F', 'S', '1', 0, 0};

static void pack_field(char* dest, size_t field_len, const char* src) {
    memset(dest, 0, field_len);
    if (!src) {
        return;
    }

    for (size_t i = 0; i < field_len && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
}

static void unpack_field(char* dest, size_t dest_len, const char* src, size_t field_len) {
    size_t i = 0;

    if (dest_len == 0) {
        return;
    }

    while (i + 1 < dest_len && i < field_len && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

static int is_valid_entry(const nawfs_entry* entry) {
    if (entry->name[0] == '\0') {
        return 0;
    }

    if (entry->sector < DATA_START_SECTOR) {
        return 0;
    }

    if (entry->sector >= DATA_START_SECTOR + DATA_SECTOR_COUNT) {
        return 0;
    }

    if (entry->size > MAX_FILE_SIZE) {
        return 0;
    }

    return 1;
}

static int entry_matches(const nawfs_entry* entry, const char* name, const char* ext) {
    char packed_name[FILENAME_LEN];
    char packed_ext[EXTENSION_LEN];

    pack_field(packed_name, sizeof(packed_name), name);
    pack_field(packed_ext, sizeof(packed_ext), ext);

    return memcmp(entry->name, packed_name, sizeof(packed_name)) == 0 &&
           memcmp(entry->ext, packed_ext, sizeof(packed_ext)) == 0;
}

static int find_free_sector(void) {
    for (int sector = DATA_START_SECTOR; sector < DATA_START_SECTOR + DATA_SECTOR_COUNT; sector++) {
        int in_use = 0;

        for (int i = 0; i < file_count; i++) {
            if (entries[i].sector == sector) {
                in_use = 1;
                break;
            }
        }

        if (!in_use) {
            return sector;
        }
    }

    return -1;
}

static void build_superblock(nawfs_superblock* superblock) {
    memset(superblock, 0, sizeof(*superblock));
    memcpy(superblock->magic, nawfs_magic, sizeof(superblock->magic));
    superblock->version = NAWFS_VERSION;
    superblock->max_files = MAX_FILES;
    superblock->catalog_start_sector = CATALOG_START_SECTOR;
    superblock->catalog_sectors = CATALOG_SECTORS;
    superblock->data_start_sector = DATA_START_SECTOR;
    superblock->data_sector_count = DATA_SECTOR_COUNT;
}

static int superblock_is_valid(const nawfs_superblock* superblock) {
    if (memcmp(superblock->magic, nawfs_magic, sizeof(superblock->magic)) != 0) {
        return 0;
    }

    if (superblock->version != NAWFS_VERSION) {
        return 0;
    }

    if (superblock->max_files != MAX_FILES) {
        return 0;
    }

    if (superblock->catalog_start_sector != CATALOG_START_SECTOR) {
        return 0;
    }

    if (superblock->catalog_sectors != CATALOG_SECTORS) {
        return 0;
    }

    if (superblock->data_start_sector != DATA_START_SECTOR) {
        return 0;
    }

    if (superblock->data_sector_count != DATA_SECTOR_COUNT) {
        return 0;
    }

    return 1;
}

static int load_catalog(void) {
    uint8_t sector_data[SECTOR_SIZE];

    file_count = 0;
    memset(entries, 0, sizeof(entries));

    for (int i = 0; i < CATALOG_SECTORS; i++) {
        memset(sector_data, 0, sizeof(sector_data));

        if (disk_read_sector(CATALOG_START_SECTOR + i, sector_data) != 0) {
            print("FS: failed to read catalog sector\n");
            return -1;
        }

        for (int j = 0; j < SECTOR_SIZE / sizeof(nawfs_entry); j++) {
            nawfs_entry* e = (nawfs_entry*)(sector_data + j * sizeof(nawfs_entry));
            if (is_valid_entry(e) && file_count < MAX_FILES) {
                entries[file_count++] = *e;
            }
        }
    }

    return 0;
}

int fs_format() {
    nawfs_superblock superblock;
    uint8_t zero_sector[SECTOR_SIZE];

    build_superblock(&superblock);
    memset(zero_sector, 0, sizeof(zero_sector));

    if (disk_write_sector(FS_SUPERBLOCK_SECTOR, (const uint8_t*)&superblock) != 0) {
        return -1;
    }

    for (int i = 0; i < CATALOG_SECTORS; i++) {
        if (disk_write_sector(CATALOG_START_SECTOR + i, zero_sector) != 0) {
            return -1;
        }
    }

    for (int i = 0; i < DATA_SECTOR_COUNT; i++) {
        if (disk_write_sector(DATA_START_SECTOR + i, zero_sector) != 0) {
            return -1;
        }
    }

    file_count = 0;
    memset(entries, 0, sizeof(entries));
    return 0;
}

void fs_init() {
    nawfs_superblock superblock;

    asm volatile("cli");

    if (disk_read_sector(FS_SUPERBLOCK_SECTOR, (uint8_t*)&superblock) != 0) {
        print("FS: disk is unavailable\n");
        asm volatile("sti");
        return;
    }

    if (!superblock_is_valid(&superblock)) {
        print("FS: storage not found, formatting...\n");
        if (fs_format() != 0) {
            print("FS: format failed\n");
            asm volatile("sti");
            return;
        }
        print("FS: storage is ready\n");
    }

    if (load_catalog() != 0) {
        asm volatile("sti");
        return;
    }

    asm volatile("sti");
}

void fs_flush() {
    uint8_t buffer[SECTOR_SIZE];

    for (int i = 0; i < CATALOG_SECTORS; i++) {
        memset(buffer, 0, sizeof(buffer));

        for (int j = 0; j < SECTOR_SIZE / sizeof(nawfs_entry); j++) {
            int idx = i * (SECTOR_SIZE / sizeof(nawfs_entry)) + j;
            if (idx < file_count) {
                memcpy(buffer + j * sizeof(nawfs_entry), &entries[idx], sizeof(nawfs_entry));
            }
        }

        if (disk_write_sector(CATALOG_START_SECTOR + i, buffer) != 0) {
            print("FS: failed to write catalog sector\n");
            return;
        }
    }
}

int find_file(const char* name, const char* ext) {
    for (int i = 0; i < file_count; i++) {
        if (entry_matches(&entries[i], name, ext)) {
            return i;
        }
    }
    return -1;
}

int fs_read_to_buffer(const char* name, const char* ext, char* buffer, int max_len) {
    const char* file_data = fs_read(name, ext);
    if (!file_data || !buffer || max_len <= 0) return -1;
   
    int i;

    for (i = 0; i < max_len - 1 && file_data[i] != '\0'; i++) {
        buffer[i] = file_data[i];
    }
        buffer[i] = '\0';
    return i;
}

int fs_create(const char* name, const char* ext) {
    int free_sector;
    uint8_t zero_sector[SECTOR_SIZE];

    if (file_count >= MAX_FILES) return -1;
    if (!name || !ext || name[0] == '\0' || ext[0] == '\0') return -1;
    if (find_file(name, ext) >= 0) return -1;

    free_sector = find_free_sector();
    if (free_sector < 0) return -1;

    nawfs_entry* e = &entries[file_count];
    memset(e, 0, sizeof(nawfs_entry));
    strncpy(e->name, name, FILENAME_LEN);
    strncpy(e->ext, ext, EXTENSION_LEN);
    e->sector = (uint16_t)free_sector;
    e->size = 0;

    memset(zero_sector, 0, sizeof(zero_sector));
    if (disk_write_sector(e->sector, zero_sector) != 0) {
        return -1;
    }

    file_count++;
    fs_flush();
    return 0;
}

int fs_write(const char* name, const char* ext, const char* data) {
    int i = find_file(name, ext);
    uint16_t len;

    if (i < 0) return -1;
    if (!data) return -1;

    len = (uint16_t)strlen(data);
    if (len > MAX_FILE_SIZE) len = MAX_FILE_SIZE;

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
    static char buffer[MAX_FILE_SIZE + 1];

    for (int i = 0; i < file_count; ++i) {
        uint16_t size;

        if (!entry_matches(&entries[i], name, ext)) {
            continue;
        }

        if (disk_read_sector(entries[i].sector, (uint8_t*)buffer) != 0) {
            return NULL;
        }

        size = entries[i].size;
        if (size > MAX_FILE_SIZE) {
            size = MAX_FILE_SIZE;
        }

        buffer[size] = '\0';
        return buffer;
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

    if (file_count > 0) {
        memset(&entries[file_count - 1], 0, sizeof(nawfs_entry));
    }

    file_count--;
    fs_flush();
    return 0;
}

void fs_list() {
    if (file_count == 0) {
        print("No files\n");
        return;
    }

    for (int i = 0; i < file_count; i++) {
        char name[FILENAME_LEN + 1];
        char ext[EXTENSION_LEN + 1];
        char size[8];

        unpack_field(name, sizeof(name), entries[i].name, FILENAME_LEN);
        unpack_field(ext, sizeof(ext), entries[i].ext, EXTENSION_LEN);

        print(" - ");
        print(name);
        print(".");
        print(ext);
        print(" (");
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


char* strncpy(char* dest, const char* src, unsigned int n) {
    unsigned int i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = 0;
    return dest;
}

unsigned int strlen(const char* s) {
    unsigned int len = 0;
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
