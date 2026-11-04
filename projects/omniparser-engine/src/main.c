#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    printf("OmniParser Engine CLI (C11 Systems Edition)\n");
    
    const char *sample_input = "let x = 42;";
    if (argc > 1) {
        sample_input = argv[1];
    }

    printf("Lexing input: \"%s\"\n", sample_input);
    omni_lexer_stream_t *stream = omni_lex_input(sample_input);
    if (!stream) {
        fprintf(stderr, "Failed to lex input stream.\n");
        return 1;
    }

    printf("Successfully generated %zu tokens.\n", stream->count);
    for (size_t i = 0; i < stream->count; i++) {
        printf("  Token[%zu]: type=%d lexeme='%s' (line %zu, col %zu)\n",
               i, stream->tokens[i].type, stream->tokens[i].lexeme,
               stream->tokens[i].line, stream->tokens[i].column);
    }

    omni_grammar_t grammar;
    omni_init_grammar(&grammar);
    int term_let = omni_add_symbol(&grammar, "let", true);
    int term_id = omni_add_symbol(&grammar, "IDENT", true);
    int term_eq = omni_add_symbol(&grammar, "=", true);
    int term_num = omni_add_symbol(&grammar, "NUMBER", true);
    int term_semi = omni_add_symbol(&grammar, ";", true);
    int nonterm_stmt = omni_add_symbol(&grammar, "Stmt", false);

    int rhs[] = {term_let, term_id, term_eq, term_num, term_semi};
    omni_add_production(&grammar, nonterm_stmt, rhs, 5);

    omni_compute_first_follow(&grammar);

    omni_parser_table_t table;
    omni_build_parser_table(&grammar, &table);

    omni_pool_t *pool = omni_pool_init(4096);
    if (!pool) {
        fprintf(stderr, "Failed to initialize memory pool.\n");
        omni_free_lexer_stream(stream);
        return 1;
    }

    omni_ast_node_t *ast = omni_parse(stream, &table, &grammar, pool);
    if (ast) {
        printf("Generated Abstract Syntax Tree:\n");
        omni_ast_print(ast, 1);
    } else {
        printf("Parsing failed to produce an AST.\n");
    }

    omni_pool_free(pool);
    omni_free_lexer_stream(stream);
    printf("OmniParser execution completed successfully.\n");
    return 0;
}
