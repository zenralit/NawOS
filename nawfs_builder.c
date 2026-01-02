#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SECTOR_SIZE 512
#define IMG_SIZE_SECTORS 65536
#define CATALOG_SECTORS 1
#define MAX_FILES_PER_SECTOR (SECTOR_SIZE / sizeof(nawfs_entry))

typedef struct {
    char name[12];      // имя файла
    uint32_t size;      // размер
    uint32_t start_lba; // сектор, где лежит файл
} __attribute__((packed)) nawfs_entry;


int main() {
    FILE *img = fopen("nawfs.img", "wb");
    if (!img) {
        perror("fopen");
        return 1;
    }

   
    uint8_t zero[SECTOR_SIZE] = {0};
    for (int i = 0; i < IMG_SIZE_SECTORS; i++)
        fwrite(zero, 1, SECTOR_SIZE, img);

   
    fseek(img, SECTOR_SIZE, SEEK_SET);

    nawfs_entry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, "hello.txt", 11);
    entry.size = 15;
    entry.start_lba = 2; 

    fwrite(&entry, sizeof(entry), 1, img);

  
    fseek(img, SECTOR_SIZE * entry.start_lba, SEEK_SET);
    const char *content = "Hello\n";
    fwrite(content, 1, entry.size, img);

    fclose(img);
    printf("Created nawfs.img with file: hello.txt\n");
    return 0;
}

