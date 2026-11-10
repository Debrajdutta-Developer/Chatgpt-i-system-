#include "../src/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main() {
    printf("Running integration suites...\n");

    // Test 1: Complex nested alternation and repetition
    {
        RegexProgram prog;
        assert(regex_compile("(a|b)*(c|d)+e?", &prog) == REGEX_OK);
        assert(regex_match(&prog, "ababccd") == true);
        assert(regex_match(&prog, "cde") == true);
        assert(regex_match(&prog, "e") == false);
        regex_free(&prog);
    }

    // Test 2: Adversarial ReDoS string immunity verification
    {
        RegexProgram prog;
        assert(regex_compile("(a*)*b", &prog) == REGEX_OK);
        char long_str[200];
        memset(long_str, 'a', 150);
        long_str[150] = 'b';
        long_str[151] = '\0';
        assert(regex_match(&prog, long_str) == true);
        regex_free(&prog);
    }

    // Test 3: Empty pattern handling
    {
        RegexProgram prog;
        assert(regex_compile("", &prog) == REGEX_ERR_EMPTY_EXPRESSION);
    }

    // Test 4: Unbalanced parentheses
    {
        RegexProgram prog;
        assert(regex_compile("a(b(c)d", &prog) == REGEX_ERR_UNBALANCED_PAREN);
    }

    // Test 5: Wildcard dot matching
    {
        RegexProgram prog;
        assert(regex_compile("a.c", &prog) == REGEX_OK);
        assert(regex_match(&prog, "abc") == true);
        assert(regex_match(&prog, "a9c") == true);
        assert(regex_match(&prog, "ac") == false);
        regex_free(&prog);
    }

    // Test 6: Character class with negation/ranges
    {
        RegexProgram prog;
        assert(regex_compile("[0-9][a-Z]", &prog) == REGEX_OK); // Note: ASCII range [a-z]
        assert(regex_compile("[0-9][a-z]", &prog) == REGEX_OK);
        assert(regex_match(&prog, "5x") == true);
        assert(regex_match(&prog, "9A") == false);
        regex_free(&prog);
    }

    // Test 7: Exact match boundaries
    {
        RegexProgram prog;
        assert(regex_compile("abc", &prog) == REGEX_OK);
        assert(regex_match(&prog, "abcd") == false);
        assert(regex_match(&prog, "fabc") == false);
        regex_free(&prog);
    }

    // Test 8: Multiple consecutive Kleene stars
    {
        RegexProgram prog;
        assert(regex_compile("a*b*c*", &prog) == REGEX_OK);
        assert(regex_match(&prog, "aaabbbccc") == true);
        assert(regex_match(&prog, "") == true);
        regex_free(&prog);
    }

    // Test 9: Long non-matching adversarial string
    {
        RegexProgram prog;
        assert(regex_compile("a(b|c)d", &prog) == REGEX_OK);
        char fail_str[500];
        memset(fail_str, 'a', 499);
        fail_str[499] = '\0';
        assert(regex_match(&prog, fail_str) == false);
        regex_free(&prog);
    }

    // Test 10: State minimization verification
    {
        RegexProgram prog;
        assert(regex_compile("(a|a|a|a|a|a)", &prog) == REGEX_OK);
        assert(prog.dfa.state_count <= 5);
        assert(regex_match(&prog, "a") == true);
        regex_free(&prog);
    }

    printf("ALL INTEGRATION TESTS PASSED.\n");
    return 0;
}
