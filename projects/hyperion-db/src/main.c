#include "engine.h"
#include <stdio.h>
#include <string.h>

#define CLI_BUFFER_SIZE 256

int main(void) {
    printf("Hyperion DB CLI v1.0 (Embedded C11 Engine)\n");
    printf("Type SQL commands or 'EXIT' to quit.\n");

    hyp_db_t *db = hyp_db_open("hyperion.db");
    if (!db) {
        fprintf(stderr, "Failed to initialize Hyperion DB.\n");
        return 1;
    }

    hyp_txn_id_t current_txn = 0;
    char line[CLI_BUFFER_SIZE];
    char output[CLI_BUFFER_SIZE];

    while (1) {
        printf("hyperion> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // Strip newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (strcmp(line, "EXIT") == 0 || strcmp(line, "exit") == 0) {
            break;
        }

        if (strlen(line) == 0) {
            continue;
        }

        bool success = hyp_execute_sql(db, &current_txn, line, output, sizeof(output));
        printf("=> %s\n", output);
        (void)success;
    }

    hyp_db_close(db);
    printf("Hyperion DB shut down cleanly.\n");
    return 0;
}
