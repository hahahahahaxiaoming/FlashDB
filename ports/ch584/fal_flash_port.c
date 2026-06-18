/*
 * Copyright (c) 2022, kaans, <https://github.com/kaans>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fal.h>
#include <sfud.h>

#define CH584_NOR_FLASH_NAME        "norflash0"
#define CH584_NOR_FLASH_SIZE        (4UL * 1024UL * 1024UL)
#define CH584_NOR_FLASH_BLK_SIZE    4096UL

static sfud_flash_t sfud_dev = NULL;

static int init(void);
static int read(long offset, uint8_t *buf, size_t size);
static int write(long offset, const uint8_t *buf, size_t size);
static int erase(long offset, size_t size);

struct fal_flash_dev nor_flash0 =
{
    .name       = CH584_NOR_FLASH_NAME,
    .addr       = 0,
    .len        = CH584_NOR_FLASH_SIZE,
    .blk_size   = CH584_NOR_FLASH_BLK_SIZE,
    .ops        = {init, read, write, erase},
    .write_gran = 1
};

static int init(void)
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

static int read(long offset, uint8_t *buf, size_t size)
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

static int write(long offset, const uint8_t *buf, size_t size)
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

static int erase(long offset, size_t size)
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
