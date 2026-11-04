#include "../src/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int integration_assertions = 0;

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "INTEGRATION FAIL: %s:%d: Condition failed: %s\n", __FILE__, __LINE__, #expr); \
        exit(1); \
    } else { \
        integration_assertions++; \
    } \
} while(0)

int main(void) {
    printf("Running OmniParser Engine Integration Tests...\n");

    /* Integration Test 1: Full pipeline lexing complex assignment */
    const char *source = "let result = 100;";
    omni_lexer_stream_t *stream = omni_lex_input(source);
    REQUIRE(stream != NULL);
    REQUIRE(stream->count >= 5);

    /* Integration Test 2: Grammar construction for assignment statement */
    omni_grammar_t g;
    omni_init_grammar(&g);
    int t_let = omni_add_symbol(&g, "let", true);
    int t_id = omni_add_symbol(&g, "IDENT", true);
    int t_eq = omni_add_symbol(&g, "=", true);
    int t_num = omni_add_symbol(&g, "NUMBER", true);
    int t_semi = omni_add_symbol(&g, ";", true);
    int nt_stmt = omni_add_symbol(&g, "Statement", false);

    REQUIRE(t_let >= 0);
    REQUIRE(nt_stmt >= 0);

    /* Integration Test 3: Production rule linking */
    int rhs_prod[] = {t_let, t_id, t_eq, t_num, t_semi};
    int prod_idx = omni_add_production(&g, nt_stmt, rhs_prod, 5);
    REQUIRE(prod_idx == 0);
    REQUIRE(g.production_count == 1);

    /* Integration Test 4: FIRST and FOLLOW set computation verification */
    omni_compute_first_follow(&g);
    REQUIRE(g.first_sets[t_let][t_let] == true);

    /* Integration Test 5: Parser table construction */
    omni_parser_table_t table;
    omni_build_parser_table(&g, &table);
    REQUIRE(table.state_count > 1);

    /* Integration Test 6: Memory pool arena instantiation */
    omni_pool_t *pool = omni_pool_init(2048);
    REQUIRE(pool != NULL);

    /* Integration Test 7: AST generation from token stream and parse table */
    omni_ast_node_t *ast = omni_parse(stream, &table, &g, pool);
    REQUIRE(ast != NULL);
    REQUIRE(ast->type == OMNI_AST_NODE_ROOT);

    /* Integration Test 8: AST node hierarchy verification */
    REQUIRE(ast->child_count > 0);
    REQUIRE(ast->children[0] != NULL);

    /* Integration Test 9: Malformed stream and recovery resilience */
    omni_lexer_stream_t *malformed_stream = omni_lex_input("let = ;");
    REQUIRE(malformed_stream != NULL);
    omni_ast_node_t *malformed_ast = omni_parse(malformed_stream, &table, &g, pool);
    REQUIRE(malformed_ast != NULL);

    /* Integration Test 10: Cleanup and resource reclamation */
    omni_pool_free(pool);
    omni_free_lexer_stream(stream);
    omni_free_lexer_stream(malformed_stream);

    printf("ALL INTEGRATION TESTS PASSED (%d assertions verified).\n", integration_assertions);
    return 0;
}
