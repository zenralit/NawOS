#include "disk.h"
#include "ports.h"
#include "screen.h"
#include <stdint.h>

#define ATA_PRIMARY_IO     0x1F0
#define ATA_PRIMARY_CTRL   0x3F6
#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_STATUS_BSY     0x80
#define ATA_STATUS_DRQ     0x08
#define ATA_SR_BSY         0x80
#define ATA_SR_DRQ         0x08
#define ATA_SR_ERR         0x01

int ata_wait() {
    int timeout = 100000;
    uint8_t status;

    
    while (((status = inb(ATA_PRIMARY_IO + 7)) & ATA_SR_BSY) && --timeout);
    if (timeout <= 0) {
        print("ATA timeout: BSY stuck\n");
        return -1;
    }

    timeout = 100000;
    
    while (!((status = inb(ATA_PRIMARY_IO + 7)) & ATA_SR_DRQ) && --timeout);
    if (timeout <= 0) {
       // print("ATA timeout: DRQ not ready\n");
        return -2;
    }

    if (status & ATA_SR_ERR) {
        print("ATA error: ERR set in status register\n");
        return -3;
    }

    return 0;
}

int disk_read_sector(int lba, uint8_t* buffer) {
    if (lba < 0 || lba > 1024) {
        print("disk_read_sector: LBA out of range\n");
        return -1;
    }

    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F)); 
    outb(ATA_PRIMARY_IO + 2, 1);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)(lba));
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + 7, ATA_CMD_READ_PIO);
    
    if (ata_wait() != 0) {
        return -2;
    }

    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(ATA_PRIMARY_IO);
        buffer[i * 2]     = data & 0xFF;
        buffer[i * 2 + 1] = (data >> 8) & 0xFF;
    }

    return 0;
}

int disk_write_sector(int lba, const uint8_t* buffer) {
    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + 2, 1);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)(lba));
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + 7, ATA_CMD_WRITE_PIO);

        ata_wait();

    for (int i = 0; i < 256; i++) {
        uint16_t data = ((uint16_t)buffer[i * 2 + 1] << 8) | buffer[i * 2];
        outw(ATA_PRIMARY_IO, data);
    }

    
    outb(ATA_PRIMARY_IO + 7, 0xE7);  
        ata_wait();

    return 0;
}