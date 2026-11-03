#include "../src/engine.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("Running Hyperion Integration & SQL Tests...\n");

    hyp_db_t *db = hyp_db_open("integration.db");
    assert(db != NULL); // Check 1

    hyp_txn_id_t current_txn = 0;
    char output[256];

    // Check 2: BEGIN SQL execution
    bool r1 = hyp_execute_sql(db, &current_txn, "BEGIN", output, sizeof(output));
    assert(r1 == true);
    assert(current_txn > 0); // Check 3

    // Check 4: CREATE TABLE SQL execution
    bool r2 = hyp_execute_sql(db, &current_txn, "CREATE TABLE accounts", output, sizeof(output));
    assert(r2 == true);

    // Check 5: INSERT INTO SQL execution
    bool r3 = hyp_execute_sql(db, &current_txn, "INSERT INTO accounts VALUES (10, 5000)", output, sizeof(output));
    assert(r3 == true);

    // Check 6: COMMIT SQL execution
    bool r4 = hyp_execute_sql(db, &current_txn, "COMMIT", output, sizeof(output));
    assert(r4 == true);

    // Check 7: SELECT SQL execution
    bool r5 = hyp_execute_sql(db, &current_txn, "SELECT * FROM accounts WHERE key=10", output, sizeof(output));
    assert(r5 == true);

    // Check 8: BEGIN and ROLLBACK lifecycle
    bool r6 = hyp_execute_sql(db, &current_txn, "BEGIN", output, sizeof(output));
    assert(r6 == true);

    // Check 9: ROLLBACK execution
    bool r7 = hyp_execute_sql(db, &current_txn, "ROLLBACK", output, sizeof(output));
    assert(r7 == true);

    // Check 10: Syntax error handling
    bool r8 = hyp_execute_sql(db, &current_txn, "INVALID SQL STATEMENT", output, sizeof(output));
    assert(r8 == false);

    hyp_db_close(db);
    printf("All 10 integration test assertions passed successfully!\n");
    return 0;
}
