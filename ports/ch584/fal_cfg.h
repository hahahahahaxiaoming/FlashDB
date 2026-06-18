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

/* ===================== Flash device Configuration ========================= */
extern struct fal_flash_dev nor_flash0;

/* flash device table */
#define FAL_FLASH_DEV_TABLE                                          \
{                                                                    \
    &nor_flash0,                                                     \
}

/* ====================== Partition Configuration ========================== */
#ifdef FAL_PART_HAS_TABLE_CFG
/* partition table */
#define FAL_PART_TABLE                                                                \
{                                                                                     \
    {FAL_PART_MAGIC_WORD,  "fdb_kvdb1",    "norflash0",     512*1024,  16*1024, 0},  \
    {FAL_PART_MAGIC_WORD,  "fdb_tsdb1",    "norflash0",     528*1024,  16*1024, 0},  \
}
#endif /* FAL_PART_HAS_TABLE_CFG */

#endif /* _FAL_CFG_H_ */
