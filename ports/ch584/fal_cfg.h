/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

// #define FAL_PRINTF   // 定义 FAL_PRINTF 为空，禁用 FAL 内部日志输出 
#define FAL_DEBUG 0     // 1: 打印调试信息，0: 不打印
#define FAL_PART_HAS_TABLE_CFG

/*
 * Device switches:
 * - CH584_onchip: CH584 internal Flash-ROM, 448KB total.
 * - norflash0: external W25Q32 via SFUD.
 *
 * FLASHDB_FDB_ONCHIP selects where the current KVDB/TSDB partitions are placed.
 * Both devices can be registered at the same time by enabling both switches.
 */
#ifndef FLASHDB_FDB_ONCHIP
#define FLASHDB_FDB_ONCHIP             1
#endif

#ifndef FLASHDB_ENABLE_ONCHIP_FLASH
#define FLASHDB_ENABLE_ONCHIP_FLASH    1
#endif

#ifndef FLASHDB_ENABLE_NORFLASH0
#define FLASHDB_ENABLE_NORFLASH0       0
#endif

#if FLASHDB_FDB_ONCHIP && !FLASHDB_ENABLE_ONCHIP_FLASH
#error "FLASHDB_FDB_ONCHIP requires FLASHDB_ENABLE_ONCHIP_FLASH"
#endif

#if !FLASHDB_FDB_ONCHIP && !FLASHDB_ENABLE_NORFLASH0
#error "External FlashDB partitions require FLASHDB_ENABLE_NORFLASH0"
#endif

/* ===================== Flash device Configuration ========================= */
#if FLASHDB_ENABLE_ONCHIP_FLASH
extern struct fal_flash_dev ch584_onchip_flash;
#endif

#if FLASHDB_ENABLE_NORFLASH0
extern struct fal_flash_dev nor_flash0;
#endif

/* flash device table */
#if FLASHDB_ENABLE_ONCHIP_FLASH && FLASHDB_ENABLE_NORFLASH0
#define FAL_FLASH_DEV_TABLE                                          \
{                                                                    \
    &ch584_onchip_flash,                                             \
    &nor_flash0,                                                     \
}
#elif FLASHDB_ENABLE_ONCHIP_FLASH
#define FAL_FLASH_DEV_TABLE                                          \
{                                                                    \
    &ch584_onchip_flash,                                             \
}
#elif FLASHDB_ENABLE_NORFLASH0
#define FAL_FLASH_DEV_TABLE                                          \
{                                                                    \
    &nor_flash0,                                                     \
}
#endif

/* ====================== Partition Configuration ========================== */
#ifdef FAL_PART_HAS_TABLE_CFG
#if FLASHDB_FDB_ONCHIP
#define FAL_PART_TABLE                                                                    \
{                                                                                         \
    {FAL_PART_MAGIC_WORD,  "fdb_kvdb1",    "CH584_onchip",     416 * 1024, 16 * 1024, 0}, \
    {FAL_PART_MAGIC_WORD,  "fdb_tsdb1",    "CH584_onchip",     432 * 1024, 16 * 1024, 0}, \
}
#else
#define FAL_PART_TABLE                                                                    \
{                                                                                         \
    {FAL_PART_MAGIC_WORD,  "fdb_kvdb1",    "norflash0",        512 * 1024, 16 * 1024, 0}, \
    {FAL_PART_MAGIC_WORD,  "fdb_tsdb1",    "norflash0",        528 * 1024, 16 * 1024, 0}, \
}
#endif
#endif /* FAL_PART_HAS_TABLE_CFG */

#endif /* _FAL_CFG_H_ */
