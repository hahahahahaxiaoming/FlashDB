/*
 * Copyright (c) 2022, kaans, <https://github.com/kaans>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fal.h>
#include "fal_cfg.h"

#if FLASHDB_ENABLE_ONCHIP_FLASH
#include "CH58x_common.h"
#endif

#if FLASHDB_ENABLE_NORFLASH0
#include <sfud.h>
#endif

#define CH584_ONCHIP_FLASH_NAME        "CH584_onchip"
#define CH584_ONCHIP_FLASH_SIZE        (448UL * 1024UL)
#define CH584_ONCHIP_FLASH_BLK_SIZE    4096UL
#define CH584_ONCHIP_FLASH_WRITE_GRAN  32

#define NORFLASH0_NAME                 "norflash0"
#define NORFLASH0_SIZE                 (4UL * 1024UL * 1024UL)
#define NORFLASH0_BLK_SIZE             4096UL
#define NORFLASH0_WRITE_GRAN           1

#if FLASHDB_ENABLE_ONCHIP_FLASH
static int onchip_init(void);
static int onchip_read(long offset, uint8_t *buf, size_t size);
static int onchip_write(long offset, const uint8_t *buf, size_t size);
static int onchip_erase(long offset, size_t size);

struct fal_flash_dev ch584_onchip_flash =
{
    .name       = CH584_ONCHIP_FLASH_NAME,
    .addr       = 0,
    .len        = CH584_ONCHIP_FLASH_SIZE,
    .blk_size   = CH584_ONCHIP_FLASH_BLK_SIZE,
    .ops        = {onchip_init, onchip_read, onchip_write, onchip_erase},
    .write_gran = CH584_ONCHIP_FLASH_WRITE_GRAN
};

static int onchip_range_valid(long offset, size_t size)
{
    if (offset < 0)
    {
        return 0;
    }

    if (((uint32_t)offset > ch584_onchip_flash.len) ||
        (size > (ch584_onchip_flash.len - (uint32_t)offset)))
    {
        return 0;
    }

    return 1;
}

static int onchip_init(void)
{
    ch584_onchip_flash.len = CH584_ONCHIP_FLASH_SIZE;
    ch584_onchip_flash.blk_size = CH584_ONCHIP_FLASH_BLK_SIZE;

    return 1;
}

static int onchip_read(long offset, uint8_t *buf, size_t size)
{
    uint32_t addr;
    size_t remain;

    if ((buf == NULL) || !onchip_range_valid(offset, size))
    {
        return -1;
    }

    addr = ch584_onchip_flash.addr + (uint32_t)offset;
    remain = size;

    while (remain > 0)
    {
        uint32_t word = 0xFFFFFFFFUL;
        uint8_t *word_bytes = (uint8_t *)&word;
        uint32_t word_addr = addr & ~0x03UL;
        uint32_t word_offset = addr & 0x03UL;
        size_t chunk = 4U - word_offset;

        if (chunk > remain)
        {
            chunk = remain;
        }

        FLASH_ROM_READ(word_addr, &word, 4);

        while (chunk > 0)
        {
            *buf++ = word_bytes[word_offset++];
            addr++;
            remain--;
            chunk--;
        }
    }

    return (int)size;
}

static int onchip_write(long offset, const uint8_t *buf, size_t size)
{
    uint32_t addr;
    size_t remain;

    if ((buf == NULL) || !onchip_range_valid(offset, size))
    {
        return -1;
    }

    addr = ch584_onchip_flash.addr + (uint32_t)offset;
    remain = size;

    while (remain > 0)
    {
        uint32_t word = 0xFFFFFFFFUL;
        uint8_t *word_bytes = (uint8_t *)&word;
        uint32_t word_addr = addr & ~0x03UL;
        uint32_t word_offset = addr & 0x03UL;
        size_t chunk = 4U - word_offset;

        if (chunk > remain)
        {
            chunk = remain;
        }

        FLASH_ROM_READ(word_addr, &word, 4);

        while (chunk > 0)
        {
            word_bytes[word_offset++] = *buf++;
            addr++;
            remain--;
            chunk--;
        }

        if (FLASH_ROM_WRITE(word_addr, &word, 4) != 0)
        {
            return -1;
        }
    }

    return (int)size;
}

static int onchip_erase(long offset, size_t size)
{
    if (!onchip_range_valid(offset, size))
    {
        return -1;
    }

    if (FLASH_ROM_ERASE(ch584_onchip_flash.addr + (uint32_t)offset, (uint32_t)size) != 0)
    {
        return -1;
    }

    return (int)size;
}
#endif

#if FLASHDB_ENABLE_NORFLASH0
static sfud_flash_t sfud_dev = NULL;

static int norflash0_init(void);
static int norflash0_read(long offset, uint8_t *buf, size_t size);
static int norflash0_write(long offset, const uint8_t *buf, size_t size);
static int norflash0_erase(long offset, size_t size);

struct fal_flash_dev nor_flash0 =
{
    .name       = NORFLASH0_NAME,
    .addr       = 0,
    .len        = NORFLASH0_SIZE,
    .blk_size   = NORFLASH0_BLK_SIZE,
    .ops        = {norflash0_init, norflash0_read, norflash0_write, norflash0_erase},
    .write_gran = NORFLASH0_WRITE_GRAN
};

static int norflash0_init(void)
{
    if (sfud_dev == NULL)
    {
        if (sfud_init() != SFUD_SUCCESS)
        {
            return -1;
        }

        sfud_dev = sfud_get_device(SFUD_W25Q32_DEVICE_INDEX);
    }

    if ((sfud_dev == NULL) || !sfud_dev->init_ok)
    {
        return -1;
    }

    nor_flash0.len = sfud_dev->chip.capacity;
    nor_flash0.blk_size = sfud_dev->chip.erase_gran;

    return 1;
}

static int norflash0_read(long offset, uint8_t *buf, size_t size)
{
    if ((sfud_dev == NULL) || !sfud_dev->init_ok || (buf == NULL) || (offset < 0))
    {
        return -1;
    }

    if (sfud_read(sfud_dev, nor_flash0.addr + (uint32_t)offset, size, buf) != SFUD_SUCCESS)
    {
        return -1;
    }

    return (int)size;
}

static int norflash0_write(long offset, const uint8_t *buf, size_t size)
{
    if ((sfud_dev == NULL) || !sfud_dev->init_ok || (buf == NULL) || (offset < 0))
    {
        return -1;
    }

    if (sfud_write(sfud_dev, nor_flash0.addr + (uint32_t)offset, size, buf) != SFUD_SUCCESS)
    {
        return -1;
    }

    return (int)size;
}

static int norflash0_erase(long offset, size_t size)
{
    if ((sfud_dev == NULL) || !sfud_dev->init_ok || (offset < 0))
    {
        return -1;
    }

    if (sfud_erase(sfud_dev, nor_flash0.addr + (uint32_t)offset, size) != SFUD_SUCCESS)
    {
        return -1;
    }

    return (int)size;
}
#endif
