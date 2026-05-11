#ifndef DRIVERS_ATA_ATA_H
#define DRIVERS_ATA_ATA_H

#include <stdint.h>

int disk_read_sector(int lba, uint8_t* buffer);
int disk_write_sector(int lba, const uint8_t* buffer);

#endif
