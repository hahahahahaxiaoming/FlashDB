# FlashDB 测试用例说明文档

## 1. 文档目的

本文档用于说明 `UserFlashDB_TestAll()` 测试函数的作用、测试流程和预期现象，方便后续在 CH584、STM32、nRF54 等平台移植 FlashDB 后快速验证功能是否正常。

该测试主要验证两部分功能：

- TSDB：日志写入、清空、计数、读取最新日志、正序遍历、倒序遍历、按游标读取。
- KVDB：结构体参数写入、读取和数据一致性校验。

## 2. 测试前提

运行该测试前，需要确认以下功能已经正常：

```text
1. SFUD 初始化成功，外部 SPI Flash 可以被识别。
2. FAL 初始化成功，分区表可以正常加载。
3. FlashDB KVDB 初始化成功。
4. FlashDB TSDB 初始化成功。
5. fdb_cfg.h 中 FDB_WRITE_GRAN 与底层 Flash 写入粒度一致。
```

如果使用外部 W25Q32，通常配置为：

```c
#define FDB_WRITE_GRAN 1                         // 外部 NOR Flash 写粒度按 1 bit 配置
```

如果使用 CH584 内部 Code Flash，通常配置为：

```c
#define FDB_WRITE_GRAN 32                        // 内部 Flash 最小写入单位为 4 字节
```

## 3. 测试函数入口

测试入口函数为：

```c
static void UserFlashDB_TestAll(void)
```

函数原型：

```c

static bool flashdb_test_cb(fdb_time_t time, uint8_t *data, size_t len)
{
    DBG("CB: time=%ld, len=%d\n", (long)time, (int)len);
    DBG("hex:");
    DBG_HEX(data, len);
    DBG("str:");
    DBG_CHAR(data, len);
    return false;
}

static void UserFlashDB_TestAll(void)
{
    uint8_t latest_buf[128] = {0};
    uint8_t one_buf[128] = {0};
    size_t latest_len = 0;
    size_t one_len = 0;
    fdb_time_t last_time = 0;
    fdb_time_t out_time = 0;
    size_t count = 0;
    uint8_t raw_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};

    struct {
        uint32_t magic;
        uint16_t value;
        uint8_t bytes[4];
    } kv_in = {0x46544244, 0x1234, {1, 2, 3, 4}}, kv_out = {0};

    DBG("========== FlashDB Test Begin ==========\n");

    fdb_tsdb_erase_all();
    count = tsdb_read_count();
    DBG("TSDB count after erase: %u\n", (unsigned int)count);

    tsdb_printf("FlashDB printf test %lu\n", (unsigned long)SYS_GetSysTickCnt());
    tsdb_write(raw_data, sizeof(raw_data));
    count = tsdb_read_count();
    DBG("TSDB count after write: %u\n", (unsigned int)count);

    if (tsdb_read_latest(latest_buf, &latest_len)) {
        DBG("TSDB latest len=%u data:", (unsigned int)latest_len);
        DBG_HEX(latest_buf, latest_len);
    } else {
        DBG("TSDB latest read failed\n");
    }

    DBG("TSDB read all forward:\n");
    tsdb_read_all_cb(flashdb_test_cb);

    DBG("TSDB read all reverse:\n");
    tsdb_read_all_reverse_cb(flashdb_test_cb);

    DBG("TSDB read one by cursor:\n");
    while (tsdb_read_one_cb(one_buf, &one_len, last_time, &out_time)) {
        DBG("ONE: time=%ld len=%u data:", (long)out_time, (unsigned int)one_len);
        DBG_HEX(one_buf, one_len);
        last_time = out_time;
    }

    if (fdb_kv_set_blob_with_retry(FLASH_DB_TEST, &kv_in, sizeof(kv_in))) {
        DBG("KV set OK\n");
    } else {
        DBG("KV set failed\n");
    }

    if (fdb_kv_get_blob_with_retry(FLASH_DB_TEST, &kv_out, sizeof(kv_out))) {
        DBG("KV get OK: magic=0x%08lx value=0x%04x bytes:",
            (unsigned long)kv_out.magic, kv_out.value);
        DBG_HEX(kv_out.bytes, sizeof(kv_out.bytes));
        if (memcmp(&kv_in, &kv_out, sizeof(kv_in)) == 0) {
            DBG("KV compare OK\n");
        } else {
            DBG("KV compare failed\n");
        }
    } else {
        DBG("KV get failed\n");
    }

    DBG("========== FlashDB Test End ==========\n");
}
```

## 4. 测试回调函数说明

### 4.1 `flashdb_test_cb`

```c
static bool flashdb_test_cb(fdb_time_t time, uint8_t *data, size_t len)
{
    DBG("CB: time=%ld, len=%d\n", (long)time, (int)len);
    DBG("hex:");
    DBG_HEX(data, len);
    DBG("str:");
    DBG_CHAR(data, len);
    return false;
}
```

该函数用于 TSDB 遍历回调。

每读取到一条 TSDB 日志，FlashDB 会调用该回调函数，并传入：

| 参数 | 含义 |
|---|---|
| `time` | 当前日志的时间戳 |
| `data` | 当前日志的数据指针 |
| `len` | 当前日志的数据长度 |

返回值说明：

```text
return false：继续遍历下一条日志。
return true ：停止遍历。
```

当前测试中返回 `false`，表示遍历所有日志。

## 5. 测试流程说明

### 5.1 清空 TSDB

```c
fdb_tsdb_erase_all();
count = tsdb_read_count();
DBG("TSDB count after erase: %u\n", (unsigned int)count);
```

作用：

```text
清空 TSDB 中所有历史日志，并读取清空后的日志数量。
```

预期结果：

```text
TSDB count after erase: 0
```

### 5.2 写入字符串日志

```c
tsdb_printf("FlashDB printf test %lu\n", (unsigned long)SYS_GetSysTickCnt());
```

作用：

```text
通过 printf 风格接口写入一条字符串日志。
```

该接口适合保存调试日志、错误信息、运行状态等文本内容。

### 5.3 写入原始二进制日志

```c
uint8_t raw_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
tsdb_write(raw_data, sizeof(raw_data));
```

作用：

```text
向 TSDB 写入一条原始二进制日志。
```

该接口适合保存结构体、传感器数据、状态数据等非字符串内容。

### 5.4 读取 TSDB 日志数量

```c
count = tsdb_read_count();
DBG("TSDB count after write: %u\n", (unsigned int)count);
```

作用：

```text
读取当前 TSDB 中的日志数量。
```

由于前面写入了 2 条日志，预期结果通常为：

```text
TSDB count after write: 2
```

### 5.5 读取最新一条日志

```c
if (tsdb_read_latest(latest_buf, &latest_len)) {
    DBG("TSDB latest len=%u data:", (unsigned int)latest_len);
    DBG_HEX(latest_buf, latest_len);
} else {
    DBG("TSDB latest read failed\n");
}
```

作用：

```text
读取 TSDB 中最新写入的一条日志。
```

因为最后写入的是：

```c
uint8_t raw_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
```

所以预期读取到：

```text
11 22 33 44 55
```

### 5.6 正序遍历 TSDB

```c
tsdb_read_all_cb(flashdb_test_cb);
```

作用：

```text
从旧到新遍历所有 TSDB 日志。
```

预期顺序：

```text
第 1 条：字符串日志
第 2 条：二进制日志 11 22 33 44 55
```

### 5.7 倒序遍历 TSDB

```c
tsdb_read_all_reverse_cb(flashdb_test_cb);
```

作用：

```text
从新到旧遍历所有 TSDB 日志。
```

预期顺序：

```text
第 1 条：二进制日志 11 22 33 44 55
第 2 条：字符串日志
```

### 5.8 按时间戳游标读取 TSDB

```c
while (tsdb_read_one_cb(one_buf, &one_len, last_time, &out_time)) {
    DBG("ONE: time=%ld len=%u data:", (long)out_time, (unsigned int)one_len);
    DBG_HEX(one_buf, one_len);
    last_time = out_time;
}
```

作用：

```text
从 last_time 开始，每次读取一条时间戳更大的日志。
```

该方式适合分批上传日志，例如：

```text
1. 上次上传到 time = 100。
2. 下次从 time = 100 后继续读取。
3. 每读取一条，就更新 last_time。
```

当没有新日志时，`tsdb_read_one_cb()` 返回 `false`，循环结束。

## 6. KVDB 测试说明

### 6.1 测试数据结构

```c
struct {
    uint32_t magic;
    uint16_t value;
    uint8_t bytes[4];
} kv_in = {0x46544244, 0x1234, {1, 2, 3, 4}}, kv_out = {0};
```

该结构体用于测试 KVDB 是否可以正确保存和读取二进制结构体数据。

字段说明：

| 字段 | 含义 |
|---|---|
| `magic` | 测试魔术字，用于判断数据是否完整 |
| `value` | 测试数值 |
| `bytes` | 测试字节数组 |

### 6.2 写入 KVDB

```c
if (fdb_kv_set_blob_with_retry(FLASH_DB_TEST, &kv_in, sizeof(kv_in))) {
    DBG("KV set OK\n");
} else {
    DBG("KV set failed\n");
}
```

作用：

```text
将 kv_in 结构体以 blob 形式保存到 KVDB。
```

如果写入成功，预期打印：

```text
KV set OK
```

### 6.3 读取 KVDB

```c
if (fdb_kv_get_blob_with_retry(FLASH_DB_TEST, &kv_out, sizeof(kv_out))) {
    DBG("KV get OK: magic=0x%08lx value=0x%04x bytes:",
        (unsigned long)kv_out.magic, kv_out.value);
    DBG_HEX(kv_out.bytes, sizeof(kv_out.bytes));
} else {
    DBG("KV get failed\n");
}
```

作用：

```text
从 KVDB 读取 FLASH_DB_TEST 对应的数据，并保存到 kv_out。
```

预期结果：

```text
magic = 0x46544244
value = 0x1234
bytes = 01 02 03 04
```

### 6.4 数据一致性校验

```c
if (memcmp(&kv_in, &kv_out, sizeof(kv_in)) == 0) {
    DBG("KV compare OK\n");
} else {
    DBG("KV compare failed\n");
}
```

作用：

```text
比较写入前的数据 kv_in 和读取后的数据 kv_out 是否完全一致。
```

预期结果：

```text
KV compare OK
```

## 7. 完整预期现象

如果 FlashDB 移植正常，串口日志大致应包含：

```text
========== FlashDB Test Begin ==========
TSDB count after erase: 0
TSDB count after write: 2
TSDB latest len=5 data:
11 22 33 44 55
TSDB read all forward:
CB: time=xxx, len=xxx
CB: time=xxx, len=5
TSDB read all reverse:
CB: time=xxx, len=5
CB: time=xxx, len=xxx
TSDB read one by cursor:
ONE: time=xxx len=xxx data:
ONE: time=xxx len=5 data:
KV set OK
KV get OK: magic=0x46544244 value=0x1234 bytes:
01 02 03 04
KV compare OK
========== FlashDB Test End ==========
```

## 8. 常见问题排查

### 8.1 TSDB 写入失败

重点检查：

```text
1. fdb_tsdb1 分区是否存在。
2. fdb_tsdb1 分区大小是否足够。
3. Flash 擦除函数是否正常。
4. Flash 写入粒度 FDB_WRITE_GRAN 是否配置正确。
```

### 8.2 KVDB 写入失败

重点检查：

```text
1. fdb_kvdb1 分区是否存在。
2. KVDB 分区至少建议 8KB 以上。
3. FDB_WRITE_GRAN 是否与 fal_flash_dev.write_gran 一致。
4. 底层 write 函数返回值是否正确。
```

### 8.3 读取数据和写入数据不一致

重点检查：

```text
1. 写入前是否已经擦除对应区域。
2. write 函数是否正确返回写入长度。
3. read 函数是否读取了正确偏移。
4. 分区 offset 是否配置错误。
```

### 8.4 重启后数据丢失

重点检查：

```text
1. 是否每次启动都调用了 fdb_tsdb_erase_all()。
2. 是否误擦除了 FlashDB 分区。
3. FlashDB 分区是否和其他数据区域重叠。
```

## 9. 使用建议

调试阶段可以保留：

```text
fdb_tsdb_erase_all()
DBG_HEX()
DBG_CHAR()
```

正式版本建议：

```text
1. 不要每次启动都清空 TSDB。
2. TSDB 分区根据日志量适当加大。
3. KVDB 只保存配置参数，不建议频繁写入。
4. 日志上传后，可以按时间戳记录上传进度。
```

