#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <pattern> <text>\n", argv[0]);
        return 1;
    }

    const char *pattern = argv[1];
    const char *text = argv[2];

    RegexProgram prog;
    RegexError err = regex_compile(pattern, &prog);
    if (err != REGEX_OK) {
        fprintf(stderr, "Compilation error: %d\n", err);
        return 1;
    }

    size_t match_len = 0;
    bool matched = regex_match_ex(&prog, text, &match_len);
    if (matched) {
        printf("MATCH (len=%zu)\n", match_len);
    } else {
        printf("NO MATCH\n");
    }

    regex_free(&prog);
    return 0;
}
