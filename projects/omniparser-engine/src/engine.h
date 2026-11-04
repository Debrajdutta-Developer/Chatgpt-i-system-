#ifndef OMNI_ENGINE_H
#define OMNI_ENGINE_H

#include <stddef.h>
#include <stdbool.h>

#define OMNI_MAX_SYMBOLS 64
#define OMNI_MAX_PRODS 128
#define OMNI_MAX_STATES 256
#define OMNI_MAX_TOKEN_LEN 256
#define OMNI_POOL_BLOCK_SIZE 4096

typedef enum {
    OMNI_TOK_EOF = 0,
    OMNI_TOK_IDENTIFIER,
    OMNI_TOK_NUMBER,
    OMNI_TOK_KEYWORD,
    OMNI_TOK_OPERATOR,
    OMNI_TOK_LITERAL,
    OMNI_TOK_UNKNOWN
} omni_token_type_t;

typedef struct {
    omni_token_type_t type;
    char lexeme[OMNI_MAX_TOKEN_LEN];
    size_t line;
    size_t column;
} omni_token_t;

typedef struct {
    omni_token_t *tokens;
    size_t count;
    size_t capacity;
    size_t index;
} omni_lexer_stream_t;

typedef struct omni_pool_block {
    char data[OMNI_POOL_BLOCK_SIZE];
    size_t used;
    struct omni_pool_block *next;
} omni_pool_block_t;

typedef struct {
    omni_pool_block_t *head;
    size_t block_size;
} omni_pool_t;

typedef enum {
    OMNI_AST_NODE_ROOT = 0,
    OMNI_AST_NODE_EXPR,
    OMNI_AST_NODE_TERM,
    OMNI_AST_NODE_FACTOR,
    OMNI_AST_NODE_DECL
} omni_ast_type_t;

typedef struct omni_ast_node {
    omni_ast_type_t type;
    char name[OMNI_MAX_TOKEN_LEN];
    omni_token_t token;
    struct omni_ast_node **children;
    size_t child_count;
    size_t child_capacity;
} omni_ast_node_t;

typedef struct {
    int lhs;
    int rhs[OMNI_MAX_SYMBOLS];
    size_t rhs_len;
} omni_production_t;

typedef struct {
    char symbol_names[OMNI_MAX_SYMBOLS][OMNI_MAX_TOKEN_LEN];
    size_t symbol_count;
    omni_production_t productions[OMNI_MAX_PRODS];
    size_t production_count;
    bool is_terminal[OMNI_MAX_SYMBOLS];
    int start_symbol;
    bool first_sets[OMNI_MAX_SYMBOLS][OMNI_MAX_SYMBOLS];
    bool follow_sets[OMNI_MAX_SYMBOLS][OMNI_MAX_SYMBOLS];
} omni_grammar_t;

typedef enum {
    OMNI_ACTION_ERROR = 0,
    OMNI_ACTION_SHIFT,
    OMNI_ACTION_REDUCE,
    OMNI_ACTION_ACCEPT
} omni_action_type_t;

typedef struct {
    omni_action_type_t type;
    int target;
} omni_table_cell_t;

typedef struct {
    omni_table_cell_t actions[OMNI_MAX_STATES][OMNI_MAX_SYMBOLS];
    int gotos[OMNI_MAX_STATES][OMNI_MAX_SYMBOLS];
    size_t state_count;
    int conflict_count;
} omni_parser_table_t;

/* Lexer API */
omni_lexer_stream_t *omni_lex_input(const char *input);
void omni_free_lexer_stream(omni_lexer_stream_t *stream);

/* Memory Pool API */
omni_pool_t *omni_pool_init(size_t block_size);
void *omni_pool_alloc(omni_pool_t *pool, size_t size);
void omni_pool_free(omni_pool_t *pool);

/* AST API */
omni_ast_node_t *omni_ast_create_node(omni_pool_t *pool, omni_ast_type_t type, const char *name);
void omni_ast_add_child(omni_pool_t *pool, omni_ast_node_t *parent, omni_ast_node_t *child);
void omni_ast_print(const omni_ast_node_t *node, int indent);

/* Grammar & Table API */
void omni_init_grammar(omni_grammar_t *g);
int omni_add_symbol(omni_grammar_t *g, const char *name, bool is_terminal);
int omni_add_production(omni_grammar_t *g, int lhs, const int *rhs, size_t rhs_len);
void omni_compute_first_follow(omni_grammar_t *g);
void omni_build_parser_table(const omni_grammar_t *g, omni_parser_table_t *table);

/* Parsing & Recovery API */
omni_ast_node_t *omni_parse(omni_lexer_stream_t *stream, const omni_parser_table_t *table, const omni_grammar_t *g, omni_pool_t *pool);

#endif
