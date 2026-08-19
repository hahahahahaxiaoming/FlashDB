#include "user_flashdb.h"
#include "CONFIG.h"
#include <stdarg.h>

#define TAG "flashdb"
#define FLASHDB_DBG(format, ...) DBG("[" TAG "] " format "\n", ##__VA_ARGS__)
#define FLASHDB_DBG_HEX(data, len) DBG_HEX(data, len)

#ifndef USER_FLASHDB_USING_LOCK
#define USER_FLASHDB_USING_LOCK 0
#endif

// 宏定义控制是否打印全部 TSDB 数据
#define TSDB_PRINT_ALL 0  // 1: 打印，0: 不打印

static uint32_t boot_count = 0;
static time_t boot_time[10] = {0, 1, 2, 3};
/* default KV nodes */
static struct fdb_default_kv_node default_kv_table[] = {
    {"username", "armink", 0},                       /* string KV */
    {"password", "123456", 0},                       /* string KV */
    {"boot_count", &boot_count, sizeof(boot_count)}, /* int type KV */
    {"boot_time", &boot_time, sizeof(boot_time)},    /* int array type KV */
};
/* KVDB object */
static struct fdb_kvdb kvdb = {0};
/* TSDB object */
struct fdb_tsdb tsdb = {0};
/* counts for simulated timestamp */
static int counts = 0;

#if USER_FLASHDB_USING_LOCK
static uint32_t flashdb_irq_status;

static void lock(fdb_db_t db)
{
    (void)db;
    SYS_DisableAllIrq(&flashdb_irq_status);
}

static void unlock(fdb_db_t db)
{
    (void)db;
    SYS_RecoverIrq(flashdb_irq_status);
}
#endif

static fdb_time_t get_time(void)
{
    /* Using the counts instead of timestamp.
     * Please change this function to return RTC time.
     */
    return ++counts;
}

int flashdb_init(void)
{
    fdb_err_t result;

#ifdef FDB_USING_KVDB
    { /* KVDB Sample */
        struct fdb_default_kv default_kv;

        default_kv.kvs = default_kv_table;
        default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);
        /* set the lock and unlock function if you want */
#if USER_FLASHDB_USING_LOCK
        fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_LOCK, lock);
        fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_UNLOCK, unlock);
#endif
        /* Key-Value database initialization
         *
         *       &kvdb: database object
         *       "env": database name
         * "fdb_kvdb1": The flash partition name base on FAL. Please make sure it's in FAL partition table.
         *              Please change to YOUR partition name.
         * &default_kv: The default KV nodes. It will auto add to KVDB when first initialize successfully.
         *        NULL: The user data if you need, now is empty.
         */
        result = fdb_kvdb_init(&kvdb, "env", "fdb_kvdb1", &default_kv, NULL);

        if (result != FDB_NO_ERR)
        {
            return -1;
        }
        // fdb_kv_set_default(&kvdb);  // 重置整个 KVDB
    }
#endif /* FDB_USING_KVDB */

#ifdef FDB_USING_TSDB
    { /* TSDB Sample */
        /* set the lock and unlock function if you want */
#if USER_FLASHDB_USING_LOCK
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_LOCK, lock);
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_UNLOCK, unlock);
#endif
        /* Time series database initialization
         *
         *       &tsdb: database object
         *       "log": database name
         * "fdb_tsdb1": The flash partition name base on FAL. Please make sure it's in FAL partition table.
         *              Please change to YOUR partition name.
         *    get_time: The get current timestamp function.
         *         128: maximum length of each log
         *        NULL: The user data if you need, now is empty.
         */
        result = fdb_tsdb_init(&tsdb, "log", "fdb_tsdb1", get_time, 128, NULL);
        /* read last saved time for simulated timestamp */
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_GET_LAST_TIME, &counts);

        if (result != FDB_NO_ERR)
        {
            return -1;
        }
        // fdb_tsl_clean(&tsdb);  // 清空 TSDB
    }
#endif /* FDB_USING_TSDB */

    return 0;
}

void fdb_tsdb_erase_all(void)
{
    fdb_tsl_clean(&tsdb);  // 清空 TSDB
}

#define TSDB_PRINTF_BUFF_SIZE 128  // 每条日志最大长度，与初始化 TSDB 时保持一致

void tsdb_printf(const char *fmt, ...)
{
    char buf[TSDB_PRINTF_BUFF_SIZE];
    va_list args;
    va_start(args, fmt);

    // 格式化字符串
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0) return; // 格式化出错
    if (len > TSDB_PRINTF_BUFF_SIZE) len = TSDB_PRINTF_BUFF_SIZE;

    // 打印到串口（可选）
    // FLASHDB_DBG("%.*s\n", len, buf);

    // 封装成 fdb_blob 并写入 TSDB
    struct fdb_blob blob = {0};
    fdb_err_t err = fdb_tsl_append(&tsdb, fdb_blob_make(&blob, (uint8_t*)buf, len));
    if (err != FDB_NO_ERR) {
        FLASHDB_DBG("Failed to append log to TSDB error: %d", err);
    }
}

void tsdb_write(uint8_t *data, size_t len)
{
    struct fdb_blob blob = {0};

    if (len > TSDB_PRINTF_BUFF_SIZE) len = TSDB_PRINTF_BUFF_SIZE;

    // FLASHDB_DBG_HEX(data, len);

    fdb_err_t err = fdb_tsl_append(&tsdb, fdb_blob_make(&blob, data, len));
    if (err != FDB_NO_ERR) {
        FLASHDB_DBG("Failed to append log to TSDB error: %d", err);
    }
}

// =================== flash kv ======================
#define FDB_MAX_RETRY        3

/**
 * @brief 写入任意二进制数据（支持结构体），带重试
 * 
 * @param key 键名
 * @param data 数据指针
 * @param length 数据长度
 * @return true 成功
 * @return false 失败
 */
bool fdb_kv_set_blob_with_retry(const char *key, const void *data, size_t length)
{
    fdb_err_t err;

    for (int attempt = 1; attempt <= FDB_MAX_RETRY; attempt++) {
        struct fdb_blob blob;
        fdb_blob_make(&blob, data, length);

        err = fdb_kv_set_blob(&kvdb, key, &blob);
        if (err == FDB_NO_ERR) {
            FLASHDB_DBG("FDB write success, key=%s len=%d (attempt %d)", key, (int)length, attempt);
            return true;
        } else {
            FLASHDB_DBG("[%d/%d] fdb_kv_set_blob failed: %d", attempt, FDB_MAX_RETRY, err);
        }
    }

    FLASHDB_DBG("FDB write failed, key=%s after %d retries", key, FDB_MAX_RETRY);
    return false;
}

/**
 * @brief 读取任意二进制数据（支持结构体），带重试
 * 
 * @param key 键名
 * @param data 输出缓冲区
 * @param length 数据长度
 * @return true 成功
 * @return false 失败或 key 不存在
 */
bool fdb_kv_get_blob_with_retry(const char *key, void *data, size_t length)
{
    for (int attempt = 1; attempt <= FDB_MAX_RETRY; attempt++) {
        struct fdb_blob blob;
        fdb_blob_make(&blob, data, length);

        int read_len = fdb_kv_get_blob(&kvdb, key, &blob);
        if (read_len > 0) {
            FLASHDB_DBG("FDB read success, key=%s len=%d (attempt %d)", key, read_len, attempt);
            return true;
        } else if (read_len == 0) {
            FLASHDB_DBG("FDB key=%s not found", key);
            return false;
        } else {
            FLASHDB_DBG("[%d/%d] fdb_kv_get_blob failed (len=%d)", attempt, FDB_MAX_RETRY, read_len);
        }
    }

    FLASHDB_DBG("FDB read failed, key=%s after %d retries", key, FDB_MAX_RETRY);
    return false;
}
// ========================== end ===========================

#if TSDB_PRINT_ALL
/* ---- 遍历回调函数 ---- */
static bool query_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob = {0};
    uint8_t buff[TSDB_PRINTF_BUFF_SIZE] = {0};
    fdb_tsdb_t db = arg;

    fdb_blob_read((fdb_db_t) db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, buff, tsl->log_len)));

    // FLASHDB_DBG("[time: %lld] DATA:%s", tsl->time, buff);
    FLASHDB_DBG("[time: %ld]", (long)tsl->time);
    FLASHDB_DBG_HEX(buff, tsl->log_len);
    return false;
}
#endif

/* ---- 获取有多少条日志 ---- */
size_t tsdb_read_count(void)
{
    size_t count;
    fdb_time_t from_time = 0;  // 起始时间
    fdb_time_t to_time   = 0x7FFFFFFF;  // 结束时间
    count = fdb_tsl_query_count(&tsdb, from_time, to_time, FDB_TSL_WRITE);
    FLASHDB_DBG("query count is: %u", count);
    return count;
}

/* ---- 逆序遍历回调函数 ---- */
static bool query_latest_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob = {0};
    uint8_t *out_buff = ((uint8_t **)arg)[0]; // 第一个指针指向缓存
    size_t *out_len   = ((size_t **)arg)[1];  // 第二个指针指向长度

    fdb_tsdb_t db = ((fdb_tsdb_t *)arg)[2];   // 第三个指针指向数据库对象

    // 读取这一条数据到缓存
    fdb_blob_read((fdb_db_t)db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, out_buff, tsl->log_len)));
    *out_len = tsl->log_len;

    // 打印调试信息
    // FLASHDB_DBG("latest record time: %ld", (long)tsl->time);
    // FLASHDB_DBG_HEX(out_buff, *out_len);

    return true;  // 返回 true 停止迭代，已经拿到最新一条
}

/* ---- 获取最新一条 ---- */
bool tsdb_read_latest(uint8_t *buff, size_t *len)
{
    void *args[3] = { buff, len, &tsdb };  // 传给回调函数的参数
    fdb_tsl_iter_reverse(&tsdb, query_latest_cb, args);
    return (*len > 0);
}

/* ---- 遍历回调函数 ---- */
static bool query_first_cb_simple(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob = {0};
    uint8_t buff[TSDB_PRINTF_BUFF_SIZE] = {0};

    // arg 是结构体指针
    struct {
        tsdb_user_cb_t user_cb;
        fdb_tsdb_t db;
    } *cb_info = (typeof(cb_info))arg;

    // 读取这一条数据到缓存
    fdb_blob_read((fdb_db_t)cb_info->db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, buff, tsl->log_len)));

    // 打印调试信息
    // FLASHDB_DBG("latest record time: %ld", (long)tsl->time);
    // FLASHDB_DBG_HEX(out_buff, *out_len);

    // 调用用户回调函数
    return cb_info->user_cb(tsl->time, buff, tsl->log_len);
}

/* ---- 获取全部（时间正序），并执行用户回调 ---- */
void tsdb_read_all_cb(tsdb_user_cb_t cb)
{
    struct {
        tsdb_user_cb_t user_cb;
        fdb_tsdb_t db;
    } cb_info = { .user_cb = cb, .db = &tsdb };

    // 回调参数直接传 struct
    fdb_tsl_iter(&tsdb, query_first_cb_simple, &cb_info);
}

/* ---- 逆序读取全部（最新 → 最老） ---- */
void tsdb_read_all_reverse_cb(tsdb_user_cb_t cb)
{
    struct {
        tsdb_user_cb_t user_cb;
        fdb_tsdb_t db;
    } cb_info = {
        .user_cb = cb,
        .db = &tsdb
    };

    /* 使用 FlashDB 的逆序遍历接口 */
    fdb_tsl_iter_reverse(&tsdb, query_first_cb_simple, &cb_info);
}

/* ---- 正序遍历，读取“下一条” ---- */
static bool query_one_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob = {0};

    struct {
        uint8_t     *buff;
        size_t      *len;
        fdb_time_t   last_time;
        fdb_time_t  *out_time;
        fdb_tsdb_t   db;
    } *ctx = arg;

    /* 1️⃣ 跳过已经读过的记录 */
    if (tsl->time <= ctx->last_time) {
        return false;   // ✅ 继续遍历（关键修正）
    }

    /* 2️⃣ 读取这一条 */
    fdb_blob_read(
        (fdb_db_t)ctx->db,
        fdb_tsl_to_blob(
            tsl,
            fdb_blob_make(&blob, ctx->buff, tsl->log_len)
        )
    );

    *ctx->len      = tsl->log_len;
    *ctx->out_time = tsl->time;

    // FLASHDB_DBG("read record time: %ld", (long)tsl->time);

    /* 3️⃣ 只读一条就停止 */
    return true;        // ✅ 停止遍历
}
/* ---- 按时间戳读取下一条 TSDB 日志 ---- */
bool tsdb_read_one_cb(uint8_t *buff,
                      size_t *len,
                      fdb_time_t last_time,
                      fdb_time_t *out_time)
{
    if (!buff || !len || !out_time) {
        return false;
    }

    *len = 0;
    *out_time = last_time;

    struct {
        uint8_t     *buff;
        size_t      *len;
        fdb_time_t   last_time;
        fdb_time_t  *out_time;
        fdb_tsdb_t   db;
    } ctx = {
        .buff      = buff,
        .len       = len,
        .last_time = last_time,
        .out_time  = out_time,
        .db        = &tsdb
    };

    fdb_tsl_iter(&tsdb, query_one_cb, &ctx);

    return (*len > 0);
}
