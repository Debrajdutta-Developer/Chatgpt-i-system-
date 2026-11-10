#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_STATES 512
#define MAX_DFA_STATES 128
#define ASCII_SIZE 256

typedef enum {
    REGEX_OK = 0,
    REGEX_ERR_UNBALANCED_PAREN,
    REGEX_ERR_UNEXPECTED_TOKEN,
    REGEX_ERR_INVALID_QUANTIFIER,
    REGEX_ERR_EMPTY_EXPRESSION,
    REGEX_ERR_DFA_OVERFLOW,
    REGEX_ERR_COMPILE_FAILED,
    REGEX_ERR_OUT_OF_MEMORY
} RegexError;

typedef enum {
    AST_LITERAL,
    AST_ANY,
    AST_CLASS,
    AST_CONCAT,
    AST_ALTERNATION,
    AST_STAR,
    AST_PLUS,
    AST_OPTIONAL
} ASTType;

typedef struct ASTNode {
    ASTType type;
    char literal;
    bool negate_class;
    uint8_t class_bits[32];
    struct ASTNode *left;
    struct ASTNode *right;
} ASTNode;

typedef struct {
    int start;
    int end;
} NFATransition;

typedef struct {
    int edge_type; // 0 = epsilon, 1 = char, 2 = any, 3 = class
    char c;
    uint8_t class_bits[32];
    int out1;
    int out2;
} NFAState;

typedef struct {
    NFAState states[MAX_STATES];
    int state_count;
    int start_state;
    int accept_state;
} NFA;

typedef struct {
    int transitions[MAX_DFA_STATES][ASCII_SIZE];
    bool is_accept[MAX_DFA_STATES];
    int start_state;
    int state_count;
} DFA;

typedef struct {
    DFA dfa;
    RegexError error;
    char err_msg[128];
} RegexProgram;

RegexError regex_compile(const char *pattern, RegexProgram *prog);
void regex_free(RegexProgram *prog);
bool regex_match(const RegexProgram *prog, const char *text);
bool regex_match_ex(const RegexProgram *prog, const char *text, size_t *match_len);

ASTNode *ast_create_literal(char c);
ASTNode *ast_create_any(void);
ASTNode *ast_create_class(const uint8_t *bits, bool negate);
ASTNode *ast_create_binary(ASTType type, ASTNode *left, ASTNode *right);
ASTNode *ast_create_unary(ASTType type, ASTNode *left);
void ast_free(ASTNode *node);

void nfa_init(NFA *nfa);
int nfa_add_state(NFA *nfa, int edge_type, char c, int out1, int out2);

int powerset_construction(const NFA *nfa, DFA *dfa);
int minimize_dfa(DFA *dfa);

#endif
