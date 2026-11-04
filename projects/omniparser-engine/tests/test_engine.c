#include "../src/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int assertions_passed = 0;

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #expr); \
        exit(1); \
    } else { \
        assertions_passed++; \
    } \
} while(0)

int main(void) {
    printf("Running OmniParser Engine Unit Tests...\n");

    /* Test 1: Lexer creation and token count validation */
    omni_lexer_stream_t *stream = omni_lex_input("fn test_func(x) { return x; }");
    ASSERT_TRUE(stream != NULL);
    ASSERT_TRUE(stream->count > 5);

    /* Test 2: Token lexeme verification */
    ASSERT_TRUE(stream->tokens[0].type == OMNI_TOK_KEYWORD);
    ASSERT_TRUE(strcmp(stream->tokens[0].lexeme, "fn") == 0);

    /* Test 3: Identifier token validation */
    ASSERT_TRUE(stream->tokens[1].type == OMNI_TOK_IDENTIFIER);
    ASSERT_TRUE(strcmp(stream->tokens[1].lexeme, "test_func") == 0);

    /* Test 4: Memory pool initialization and allocation */
    omni_pool_t *pool = omni_pool_init(1024);
    ASSERT_TRUE(pool != NULL);

    void *mem = omni_pool_alloc(pool, 128);
    ASSERT_TRUE(mem != NULL);

    /* Test 5: AST node creation and memory tracking */
    omni_ast_node_t *root = omni_ast_create_node(pool, OMNI_AST_NODE_ROOT, "TestRoot");
    ASSERT_TRUE(root != NULL);
    ASSERT_TRUE(root->type == OMNI_AST_NODE_ROOT);
    ASSERT_TRUE(strcmp(root->name, "TestRoot") == 0);

    /* Test 6: AST child addition */
    omni_ast_node_t *child = omni_ast_create_node(pool, OMNI_AST_NODE_EXPR, "ChildExpr");
    ASSERT_TRUE(child != NULL);
    omni_ast_add_child(pool, root, child);
    ASSERT_TRUE(root->child_count == 1);
    ASSERT_TRUE(root->children[0] == child);

    /* Test 7: Grammar initialization */
    omni_grammar_t grammar;
    omni_init_grammar(&grammar);
    ASSERT_TRUE(grammar.symbol_count == 0);
    ASSERT_TRUE(grammar.production_count == 0);

    /* Test 8: Symbol registration */
    int sym_term = omni_add_symbol(&grammar, "+", true);
    int sym_nonterm = omni_add_symbol(&grammar, "Expr", false);
    ASSERT_TRUE(sym_term >= 0);
    ASSERT_TRUE(sym_nonterm >= 0);
    ASSERT_TRUE(grammar.is_terminal[sym_term] == true);
    ASSERT_TRUE(grammar.is_terminal[sym_nonterm] == false);

    /* Test 9: Production addition */
    int rhs_arr[] = {sym_term};
    int prod_id = omni_add_production(&grammar, sym_nonterm, rhs_arr, 1);
    ASSERT_TRUE(prod_id >= 0);
    ASSERT_TRUE(grammar.production_count == 1);

    /* Test 10: Parser table generation */
    omni_parser_table_t table;
    omni_build_parser_table(&grammar, &table);
    ASSERT_TRUE(table.state_count > 0);

    omni_pool_free(pool);
    omni_free_lexer_stream(stream);

    printf("ALL UNIT TESTS PASSED (%d assertions verified).\n", assertions_passed);
    return 0;
}
