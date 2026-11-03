#include "../src/engine.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("Running Hyperion Engine Unit Tests...\n");

    hyp_db_t *db = hyp_db_open("test.db");
    assert(db != NULL); // Check 1

    // Check 2: Transaction begin
    hyp_txn_id_t tid = hyp_txn_begin(db);
    assert(tid > 0);

    // Check 3: Create table
    bool created = hyp_create_table(db, "users");
    assert(created == true);

    // Check 4: Duplicate table creation failure
    bool created_dup = hyp_create_table(db, "users");
    assert(created_dup == false);

    // Check 5: Insert row
    bool inserted = hyp_insert(db, tid, "users", 100, 999);
    assert(inserted == true);

    // Check 6: Select row
    int64_t val = 0;
    bool selected = hyp_select(db, "users", 100, &val);
    assert(selected == true);
    assert(val == 999); // Check 7

    // Check 8: Select non-existent row
    bool selected_missing = hyp_select(db, "users", 999, &val);
    assert(selected_missing == false);

    // Check 9: Commit transaction
    bool committed = hyp_txn_commit(db, tid);
    assert(committed == true);

    // Check 10: Crash recovery execution
    hyp_crash_recovery(db);
    assert(db->next_lsn > 1);

    hyp_db_close(db);
    printf("All 10 unit test assertions passed successfully!\n");
    return 0;
}
