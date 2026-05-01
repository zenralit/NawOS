#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "nawfs_format.h"

#define IMG_SIZE_SECTORS 1024

static void store_field(char* dest, size_t field_len, const char* src) {
    memset(dest, 0, field_len);

    for (size_t i = 0; i < field_len && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
}

static int write_file(FILE* img, const char* name, const char* ext, const char* content, uint16_t sector) {
    nawfs_entry entry;
    size_t content_len = strlen(content);

    if (content_len > MAX_FILE_SIZE) {
        content_len = MAX_FILE_SIZE;
    }

    memset(&entry, 0, sizeof(entry));
    store_field(entry.name, FILENAME_LEN, name);
    store_field(entry.ext, EXTENSION_LEN, ext);
    entry.size = (uint16_t)content_len;
    entry.sector = sector;

    if (fseek(img, SECTOR_SIZE * CATALOG_START_SECTOR, SEEK_SET) != 0) {
        return 1;
    }

    if (fwrite(&entry, sizeof(entry), 1, img) != 1) {
        return 1;
    }

    if (fseek(img, SECTOR_SIZE * sector, SEEK_SET) != 0) {
        return 1;
    }

    if (fwrite(content, 1, content_len, img) != content_len) {
        return 1;
    }

    return 0;
}

int main(int argc, char** argv) {
    const char* image_path = argc > 1 ? argv[1] : "nawfs.img";
    FILE *img = fopen(image_path, "wb");
    if (!img) {
        perror("fopen");
        return 1;
    }

    uint8_t zero[SECTOR_SIZE] = {0};
    for (int i = 0; i < IMG_SIZE_SECTORS; i++)
        fwrite(zero, 1, SECTOR_SIZE, img);

    if (write_file(img, "hello", "txt", "Hello\n", DATA_START_SECTOR) != 0) {
        fclose(img);
        fprintf(stderr, "failed to write initial file to %s\n", image_path);
        return 1;
    }

    fclose(img);
    printf("Created %s with file hello.txt at sector %d\n", image_path, DATA_START_SECTOR);
    return 0;
}
