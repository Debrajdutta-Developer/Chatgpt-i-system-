#include "../src/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static void test_ast_allocation() {
    ASTNode *lit = ast_create_literal('a');
    assert(lit != NULL);
    assert(lit->type == AST_LITERAL);
    assert(lit->literal == 'a');
    ast_free(lit);
}

static void test_ast_binary() {
    ASTNode *l = ast_create_literal('a');
    ASTNode *r = ast_create_literal('b');
    ASTNode *bin = ast_create_binary(AST_CONCAT, l, r);
    assert(bin != NULL);
    assert(bin->type == AST_CONCAT);
    assert(bin->left == l);
    assert(bin->right == r);
    ast_free(bin);
}

static void test_simple_compilation() {
    RegexProgram prog;
    RegexError err = regex_compile("abc", &prog);
    assert(err == REGEX_OK);
    assert(prog.dfa.state_count > 0);
    regex_free(&prog);
}

static void test_matching_literal() {
    RegexProgram prog;
    assert(regex_compile("hello", &prog) == REGEX_OK);
    assert(regex_match(&prog, "hello") == true);
    assert(regex_match(&prog, "hell") == false);
    assert(regex_match(&prog, "hellos") == false);
    regex_free(&prog);
}

static void test_matching_alternation() {
    RegexProgram prog;
    assert(regex_compile("cat|dog", &prog) == REGEX_OK);
    assert(regex_match(&prog, "cat") == true);
    assert(regex_match(&prog, "dog") == true);
    assert(regex_match(&prog, "bat") == false);
    regex_free(&prog);
}

static void test_matching_kleene() {
    RegexProgram prog;
    assert(regex_compile("a*b", &prog) == REGEX_OK);
    assert(regex_match(&prog, "b") == true);
    assert(regex_match(&prog, "ab") == true);
    assert(regex_match(&prog, "aaab") == true);
    assert(regex_match(&prog, "c") == false);
    regex_free(&prog);
}

static void test_matching_plus() {
    RegexProgram prog;
    assert(regex_compile("a+b", &prog) == REGEX_OK);
    assert(regex_match(&prog, "b") == false);
    assert(regex_match(&prog, "ab") == true);
    assert(regex_match(&prog, "aaab") == true);
    regex_free(&prog);
}

static void test_matching_optional() {
    RegexProgram prog;
    assert(regex_compile("a?b", &prog) == REGEX_OK);
    assert(regex_match(&prog, "b") == true);
    assert(regex_match(&prog, "ab") == true);
    assert(regex_match(&prog, "aab") == false);
    regex_free(&prog);
}

static void test_character_class() {
    RegexProgram prog;
    assert(regex_compile("[a-z]+", &prog) == REGEX_OK);
    assert(regex_match(&prog, "abcxyz") == true);
    assert(regex_match(&prog, "ABC") == false);
    regex_free(&prog);
}

static void test_malformed_pattern() {
    RegexProgram prog;
    RegexError err = regex_compile("(abc", &prog);
    assert(err != REGEX_OK);
}

int main() {
    printf("Running unit tests...");
    test_ast_allocation();
    test_ast_binary();
    test_simple_compilation();
    test_matching_literal();
    test_matching_alternation();
    test_matching_kleene();
    test_matching_plus();
    test_matching_optional();
    test_character_class();
    test_malformed_pattern();
    printf(" ALL UNIT TESTS PASSED.\n");
    return 0;
}
