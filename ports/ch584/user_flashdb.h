/*
 * FlashDB user port for CH584.
 */

#ifndef USER_FLASHDB_H
#define USER_FLASHDB_H

#include <flashdb.h>

#ifdef __cplusplus
extern "C" {
#endif

int flashdb_init(void);

void fdb_tsdb_erase_all(void);
bool tsdb_printf(const char *fmt, ...);
bool tsdb_write(uint8_t *data, size_t len);
size_t tsdb_read_count(void);

bool tsdb_read_latest(uint8_t *buff, size_t *len);
typedef bool (*tsdb_user_cb_t)(fdb_time_t time, uint8_t *data, size_t len);
void tsdb_read_all_cb(tsdb_user_cb_t cb);
void tsdb_read_all_reverse_cb(tsdb_user_cb_t cb);
bool tsdb_read_one_cb(uint8_t *buff, size_t *len, fdb_time_t last_time, fdb_time_t *out_time);

#define FLASH_DB_TEST "DBTest"
bool fdb_kv_set_blob_with_retry(const char *key, const void *data, size_t length);
bool fdb_kv_get_blob_with_retry(const char *key, void *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* USER_FLASHDB_H */
