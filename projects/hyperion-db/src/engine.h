#ifndef HYPERION_ENGINE_H
#define HYPERION_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define HYP_MAX_TABLES 16
#define HYP_MAX_COLUMNS 8
#define HYP_PAGE_SIZE 4096
#define HYP_BUFFER_POOL_SIZE 64
#define HYP_MAX_LOG_RECS 1024
#define HYP_MAX_TXNS 32
#define HYP_MAX_LOCKS 128
#define HYP_MAX_STR_LEN 32

typedef uint64_t hyp_lsn_t;
typedef uint32_t hyp_txn_id_t;
typedef uint32_t hyp_page_id_t;

typedef enum {
    HYP_LOG_BEGIN,
    HYP_LOG_COMMIT,
    HYP_LOG_ABORT,
    HYP_LOG_INSERT,
    HYP_LOG_UPDATE,
    HYP_LOG_DELETE,
    HYP_LOG_CLR
} hyp_log_type_t;

typedef struct {
    hyp_lsn_t lsn;
    hyp_lsn_t prev_lsn;
    hyp_txn_id_t txn_id;
    hyp_log_type_t type;
    hyp_page_id_t page_id;
    int64_t key;
    int64_t old_value;
    int64_t new_value;
} hyp_log_record_t;

typedef enum {
    HYP_LOCK_SHARED,
    HYP_LOCK_EXCLUSIVE
} hyp_lock_mode_t;

typedef enum {
    HYP_TXN_ACTIVE,
    HYP_TXN_COMMITTED,
    HYP_TXN_ABORTED
} hyp_txn_state_t;

typedef struct {
    hyp_txn_id_t txn_id;
    hyp_txn_state_t state;
    hyp_lsn_t last_lsn;
} hyp_txn_t;

typedef struct {
    hyp_page_id_t page_id;
    bool is_dirty;
    bool is_pinned;
    int pin_count;
    char data[HYP_PAGE_SIZE];
} hyp_frame_t;

typedef struct {
    hyp_frame_t frames[HYP_BUFFER_POOL_SIZE];
    int clock_hand;
} hyp_buffer_pool_t;

typedef struct hyp_btree_node {
    bool is_leaf;
    int num_keys;
    int64_t keys[4];
    int64_t values[4];
    struct hyp_btree_node *children[5];
    struct hyp_btree_node *next;
} hyp_btree_node_t;

typedef struct {
    char name[HYP_MAX_STR_LEN];
    hyp_btree_node_t *root;
    int row_count;
} hyp_table_t;

typedef struct {
    hyp_table_t tables[HYP_MAX_TABLES];
    int num_tables;
    hyp_buffer_pool_t bpm;
    hyp_log_record_t log_records[HYP_MAX_LOG_RECS];
    int num_log_records;
    hyp_lsn_t next_lsn;
    hyp_txn_t txns[HYP_MAX_TXNS];
    int num_txns;
    hyp_txn_id_t next_txn_id;
} hyp_db_t;

/* Database lifecycle */
hyp_db_t *hyp_db_open(const char *path);
void hyp_db_close(hyp_db_t *db);

/* Transaction management */
hyp_txn_id_t hyp_txn_begin(hyp_db_t *db);
bool hyp_txn_commit(hyp_db_t *db, hyp_txn_id_t txn_id);
bool hyp_txn_abort(hyp_db_t *db, hyp_txn_id_t txn_id);

/* DDL & DML operations */
bool hyp_create_table(hyp_db_t *db, const char *table_name);
bool hyp_insert(hyp_db_t *db, hyp_txn_id_t txn_id, const char *table_name, int64_t key, int64_t value);
bool hyp_select(hyp_db_t *db, const char *table_name, int64_t key, int64_t *out_value);

/* Recovery and Crash Simulation */
void hyp_crash_recovery(hyp_db_t *db);

/* SQL Interface */
bool hyp_execute_sql(hyp_db_t *db, hyp_txn_id_t *current_txn, const char *sql, char *output, size_t output_size);

#endif
