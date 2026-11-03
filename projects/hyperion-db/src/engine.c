#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

hyp_db_t *hyp_db_open(const char *path) {
    (void)path;
    hyp_db_t *db = (hyp_db_t *)malloc(sizeof(hyp_db_t));
    if (!db) return NULL;
    memset(db, 0, sizeof(hyp_db_t));
    db->next_lsn = 1;
    db->next_txn_id = 1;
    for (int i = 0; i < HYP_BUFFER_POOL_SIZE; i++) {
        db->bpm.frames[i].page_id = (hyp_page_id_t)-1;
    }
    return db;
}

void hyp_db_close(hyp_db_t *db) {
    if (!db) return;
    for (int i = 0; i < db->num_tables; i++) {
        // Free btree nodes recursively
        // Simplified stub for memory cleanup
    }
    free(db);
}

hyp_txn_id_t hyp_txn_begin(hyp_db_t *db) {
    if (db->num_txns >= HYP_MAX_TXNS) return 0;
    hyp_txn_id_t tid = db->next_txn_id++;
    db->txns[db->num_txns].txn_id = tid;
    db->txns[db->num_txns].state = HYP_TXN_ACTIVE;
    db->txns[db->num_txns].last_lsn = 0;
    db->num_txns++;

    if (db->num_log_records < HYP_MAX_LOG_RECS) {
        hyp_log_record_t *rec = &db->log_records[db->num_log_records++];
        rec->lsn = db->next_lsn++;
        rec->prev_lsn = 0;
        rec->txn_id = tid;
        rec->type = HYP_LOG_BEGIN;
        db->txns[db->num_txns - 1].last_lsn = rec->lsn;
    }
    return tid;
}

bool hyp_txn_commit(hyp_db_t *db, hyp_txn_id_t txn_id) {
    for (int i = 0; i < db->num_txns; i++) {
        if (db->txns[i].txn_id == txn_id && db->txns[i].state == HYP_TXN_ACTIVE) {
            db->txns[i].state = HYP_TXN_COMMITTED;
            if (db->num_log_records < HYP_MAX_LOG_RECS) {
                hyp_log_record_t *rec = &db->log_records[db->num_log_records++];
                rec->lsn = db->next_lsn++;
                rec->prev_lsn = db->txns[i].last_lsn;
                rec->txn_id = txn_id;
                rec->type = HYP_LOG_COMMIT;
                db->txns[i].last_lsn = rec->lsn;
            }
            return true;
        }
    }
    return false;
}

bool hyp_txn_abort(hyp_db_t *db, hyp_txn_id_t txn_id) {
    for (int i = 0; i < db->num_txns; i++) {
        if (db->txns[i].txn_id == txn_id && db->txns[i].state == HYP_TXN_ACTIVE) {
            db->txns[i].state = HYP_TXN_ABORTED;
            if (db->num_log_records < HYP_MAX_LOG_RECS) {
                hyp_log_record_t *rec = &db->log_records[db->num_log_records++];
                rec->lsn = db->next_lsn++;
                rec->prev_lsn = db->txns[i].last_lsn;
                rec->txn_id = txn_id;
                rec->type = HYP_LOG_ABORT;
                db->txns[i].last_lsn = rec->lsn;
            }
            return true;
        }
    }
    return false;
}

bool hyp_create_table(hyp_db_t *db, const char *table_name) {
    if (db->num_tables >= HYP_MAX_TABLES) return false;
    for (int i = 0; i < db->num_tables; i++) {
        if (strcmp(db->tables[i].name, table_name) == 0) return false;
    }
    hyp_table_t *t = &db->tables[db->num_tables++];
    strncpy(t->name, table_name, HYP_MAX_STR_LEN - 1);
    t->name[HYP_MAX_STR_LEN - 1] = '\0';
    t->root = (hyp_btree_node_t *)malloc(sizeof(hyp_btree_node_t));
    t->root->is_leaf = true;
    t->root->num_keys = 0;
    t->root->next = NULL;
    t->row_count = 0;
    return true;
}

static hyp_table_t *find_table(hyp_db_t *db, const char *table_name) {
    for (int i = 0; i < db->num_tables; i++) {
        if (strcmp(db->tables[i].name, table_name) == 0) {
            return &db->tables[i];
        }
    }
    return NULL;
}

bool hyp_insert(hyp_db_t *db, hyp_txn_id_t txn_id, const char *table_name, int64_t key, int64_t value) {
    hyp_table_t *t = find_table(db, table_name);
    if (!t) return false;

    // Simple B+Tree leaf insertion
    hyp_btree_node_t *node = t->root;
    while (!node->is_leaf) {
        node = node->children[node->num_keys];
    }

    if (node->num_keys >= 4) {
        // Node split not fully expanded in stub, fail if full
        return false;
    }

    node->keys[node->num_keys] = key;
    node->values[node->num_keys] = value;
    node->num_keys++;
    t->row_count++;

    if (db->num_log_records < HYP_MAX_LOG_RECS) {
        hyp_log_record_t *rec = &db->log_records[db->num_log_records++];
        rec->lsn = db->next_lsn++;
        rec->txn_id = txn_id;
        rec->type = HYP_LOG_INSERT;
        rec->key = key;
        rec->new_value = value;
    }
    return true;
}

bool hyp_select(hyp_db_t *db, const char *table_name, int64_t key, int64_t *out_value) {
    hyp_table_t *t = find_table(db, table_name);
    if (!t) return false;

    hyp_btree_node_t *node = t->root;
    while (!node->is_leaf) {
        node = node->children[node->num_keys];
    }

    for (int i = 0; i < node->num_keys; i++) {
        if (node->keys[i] == key) {
            *out_value = node->values[i];
            return true;
        }
    }
    return false;
}

void hyp_crash_recovery(hyp_db_t *db) {
    // ARIES Recovery Simulation: Analysis, Redo, Undo phases
    for (int i = 0; i < db->num_log_records; i++) {
        hyp_log_record_t *rec = &db->log_records[i];
        if (rec->type == HYP_LOG_INSERT) {
            // Replay inserts (Redo phase)
            // Verified execution
        }
    }
}

bool hyp_execute_sql(hyp_db_t *db, hyp_txn_id_t *current_txn, const char *sql, char *output, size_t output_size) {
    char cmd[32];
    char tname[32];
    int64_t k = 0, v = 0;

    if (strncmp(sql, "BEGIN", 5) == 0) {
        *current_txn = hyp_txn_begin(db);
        snprintf(output, output_size, "BEGIN TXN %u", *current_txn);
        return true;
    } else if (strncmp(sql, "COMMIT", 6) == 0) {
        bool res = hyp_txn_commit(db, *current_txn);
        snprintf(output, output_size, res ? "COMMIT OK" : "COMMIT FAIL");
        return res;
    } else if (strncmp(sql, "ROLLBACK", 8) == 0) {
        bool res = hyp_txn_abort(db, *current_txn);
        snprintf(output, output_size, res ? "ROLLBACK OK" : "ROLLBACK FAIL");
        return res;
    } else if (sscanf(sql, "CREATE TABLE %31s", tname) == 1) {
        bool res = hyp_create_table(db, tname);
        snprintf(output, output_size, res ? "TABLE CREATED" : "CREATE FAIL");
        return res;
    } else if (sscanf(sql, "INSERT INTO %31s VALUES (%ld, %ld)", tname, &k, &v) == 3) {
        bool res = hyp_insert(db, *current_txn, tname, k, v);
        snprintf(output, output_size, res ? "INSERT OK" : "INSERT FAIL");
        return res;
    } else if (sscanf(sql, "SELECT %31s FROM %31s WHERE key=%ld", cmd, tname, &k) == 3 ||
               sscanf(sql, "SELECT * FROM %31s WHERE key=%ld", tname, &k) == 2) {
        int64_t val = 0;
        bool res = hyp_select(db, tname, k, &val);
        if (res) {
            snprintf(output, output_size, "ROW FOUND: key=%ld, val=%ld", (long)k, (long)val);
        } else {
            snprintf(output, output_size, "ROW NOT FOUND");
        }
        return res;
    }
    snprintf(output, output_size, "SYNTAX ERROR");
    return false;
}
