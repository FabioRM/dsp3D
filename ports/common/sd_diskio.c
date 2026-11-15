/* SPDX-License-Identifier: BSD-3-Clause
 * Read-only polling SDMMC adapter. No DMA or heap dependency. */
#include "board.h"
#include "diskio.h"
#include <string.h>
static DSTATUS state = STA_NOINIT;
static uint32_t sector_buffer[128];
DSTATUS disk_initialize(BYTE drive) {
    if (drive)
        return STA_NOINIT;
    state = BSP_SD_Init() == MSD_OK ? 0 : STA_NOINIT;
    return state;
}
DSTATUS disk_status(BYTE drive) {
    if (drive)
        return STA_NOINIT;
    if (BSP_SD_IsDetected() != SD_PRESENT)
        state = STA_NOINIT | STA_NODISK;
    return state;
}
DRESULT disk_read(BYTE drive, BYTE *buffer, DWORD sector, UINT count) {
    if (!buffer || !count)
        return RES_PARERR;
    if (disk_status(drive) & STA_NOINIT)
        return RES_NOTRDY;
    for (UINT i = 0; i < count; i++) {
        if (BSP_SD_ReadBlocks(sector_buffer, sector + i, 1, 1000) != MSD_OK)
            return RES_ERROR;
        memcpy(buffer + i * 512, sector_buffer, 512);
    }
    return RES_OK;
}
DRESULT disk_write(BYTE drive, const BYTE *buffer, DWORD sector, UINT count) {
    (void)drive;
    (void)buffer;
    (void)sector;
    (void)count;
    return RES_WRPRT;
}
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buffer) {
    if (disk_status(drive) & STA_NOINIT)
        return RES_NOTRDY;
    if (cmd == CTRL_SYNC)
        return RES_OK;
    if (cmd == GET_SECTOR_SIZE && buffer) {
        *(WORD *)buffer = 512;
        return RES_OK;
    }
    return RES_PARERR;
}
