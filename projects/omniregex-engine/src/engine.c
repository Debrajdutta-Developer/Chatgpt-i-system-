#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOKEN_LITERAL,
    TOKEN_ANY,
    TOKEN_CLASS,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_STAR,
    TOKEN_PLUS,
    TOKEN_QUESTION,
    TOKEN_ALTERNATE,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char literal;
    bool negate_class;
    uint8_t class_bits[32];
} Token;

typedef struct {
    const char *pattern;
    size_t pos;
    size_t len;
    Token current;
    RegexError error;
} Lexer;

static void lexer_init(Lexer *l, const char *pattern) {
    l->pattern = pattern;
    l->pos = 0;
    l->len = strlen(pattern);
    l->error = REGEX_OK;
    memset(&l->current, 0, sizeof(Token));
}

static void lexer_next(Lexer *l) {
    if (l->pos >= l->len) {
        l->current.type = TOKEN_EOF;
        return;
    }

    char c = l->pattern[l->pos++];
    if (c == '\\') {
        if (l->pos >= l->len) {
            l->current.type = TOKEN_ERROR;
            l->error = REGEX_ERR_UNEXPECTED_TOKEN;
            return;
        }
        char esc = l->pattern[l->pos++];
        l->current.type = TOKEN_LITERAL;
        l->current.literal = esc;
        return;
    }

    if (c == '.') {
        l->current.type = TOKEN_ANY;
    } else if (c == '(') {
        l->current.type = TOKEN_LPAREN;
    } else if (c == ')') {
        l->current.type = TOKEN_RPAREN;
    } else if (c == '*') {
        l->current.type = TOKEN_STAR;
    } else if (c == '+') {
        l->current.type = TOKEN_PLUS;
    } else if (c == '?') {
        l->current.type = TOKEN_QUESTION;
    } else if (c == '|') {
        l->current.type = TOKEN_ALTERNATE;
    } else if (c == '[') {
        l->current.type = TOKEN_CLASS;
        memset(l->current.class_bits, 0, sizeof(l->current.class_bits));
        l->current.negate_class = false;

        if (l->pos < l->len && l->pattern[l->pos] == '^') {
            l->current.negate_class = true;
            l->pos++;
        }

        while (l->pos < l->len && l->pattern[l->pos] != ']') {
            char sc = l->pattern[l->pos++];
            if (sc == '\\' && l->pos < l->len) {
                sc = l->pattern[l->pos++];
            }
            if (l->pos + 1 < l->len && l->pattern[l->pos] == '-' && l->pattern[l->pos + 1] != ']') {
                l->pos++; // skip '-'
                char ec = l->pattern[l->pos++];
                for (int i = (unsigned char)sc; i <= (unsigned char)ec; ++i) {
                    l->current.class_bits[i / 8] |= (uint8_t)(1 << (i % 8));
                }
            } else {
                unsigned char idx = (unsigned char)sc;
                l->current.class_bits[idx / 8] |= (uint8_t)(1 << (idx % 8));
            }
        }
        if (l->pos < l->len && l->pattern[l->pos] == ']') {
            l->pos++;
        }
    } else {
        l->current.type = TOKEN_LITERAL;
        l->current.literal = c;
    }
}

ASTNode *ast_create_literal(char c) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = AST_LITERAL;
    node->literal = c;
    node->negate_class = false;
    memset(node->class_bits, 0, sizeof(node->class_bits));
    node->left = node->right = NULL;
    return node;
}

ASTNode *ast_create_any(void) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = AST_ANY;
    node->literal = 0;
    node->negate_class = false;
    memset(node->class_bits, 0, sizeof(node->class_bits));
    node->left = node->right = NULL;
    return node;
}

ASTNode *ast_create_class(const uint8_t *bits, bool negate) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = AST_CLASS;
    node->literal = 0;
    node->negate_class = negate;
    if (bits) {
        memcpy(node->class_bits, bits, sizeof(node->class_bits));
    } else {
        memset(node->class_bits, 0, sizeof(node->class_bits));
    }
    node->left = node->right = NULL;
    return node;
}

ASTNode *ast_create_binary(ASTType type, ASTNode *left, ASTNode *right) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    node->literal = 0;
    node->negate_class = false;
    memset(node->class_bits, 0, sizeof(node->class_bits));
    node->left = left;
    node->right = right;
    return node;
}

ASTNode *ast_create_unary(ASTType type, ASTNode *left) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    node->literal = 0;
    node->negate_class = false;
    memset(node->class_bits, 0, sizeof(node->class_bits));
    node->left = left;
    node->right = NULL;
    return node;
}

void ast_free(ASTNode *node) {
    if (!node) return;
    ast_free(node->left);
    ast_free(node->right);
    free(node);
}

static ASTNode *parse_expr(Lexer *l, RegexError *err);
static ASTNode *parse_term(Lexer *l, RegexError *err);
static ASTNode *parse_factor(Lexer *l, RegexError *err);
static ASTNode *parse_base(Lexer *l, RegexError *err);

static ASTNode *parse_base(Lexer *l, RegexError *err) {
    if (l->current.type == TOKEN_LITERAL) {
        ASTNode *n = ast_create_literal(l->current.literal);
        if (!n) { *err = REGEX_ERR_OUT_OF_MEMORY; return NULL; }
        lexer_next(l);
        return n;
    } else if (l->current.type == TOKEN_ANY) {
        ASTNode *n = ast_create_any();
        if (!n) { *err = REGEX_ERR_OUT_OF_MEMORY; return NULL; }
        lexer_next(l);
        return n;
    } else if (l->current.type == TOKEN_CLASS) {
        ASTNode *n = ast_create_class(l->current.class_bits, l->current.negate_class);
        if (!n) { *err = REGEX_ERR_OUT_OF_MEMORY; return NULL; }
        lexer_next(l);
        return n;
    } else if (l->current.type == TOKEN_LPAREN) {
        lexer_next(l);
        ASTNode *n = parse_expr(l, err);
        if (*err != REGEX_OK) return NULL;
        if (l->current.type != TOKEN_RPAREN) {
            *err = REGEX_ERR_UNBALANCED_PAREN;
            ast_free(n);
            return NULL;
        }
        lexer_next(l);
        return n;
    }
    *err = REGEX_ERR_UNEXPECTED_TOKEN;
    return NULL;
}

static ASTNode *parse_factor(Lexer *l, RegexError *err) {
    ASTNode *base = parse_base(l, err);
    if (*err != REGEX_OK) return NULL;

    if (l->current.type == TOKEN_STAR) {
        lexer_next(l);
        return ast_create_unary(AST_STAR, base);
    } else if (l->current.type == TOKEN_PLUS) {
        lexer_next(l);
        return ast_create_unary(AST_PLUS, base);
    } else if (l->current.type == TOKEN_QUESTION) {
        lexer_next(l);
        return ast_create_unary(AST_OPTIONAL, base);
    }
    return base;
}

static ASTNode *parse_term(Lexer *l, RegexError *err) {
    ASTNode *left = parse_factor(l, err);
    if (*err != REGEX_OK) return NULL;

    while (l->current.type == TOKEN_LITERAL || l->current.type == TOKEN_ANY ||
           l->current.type == TOKEN_CLASS || l->current.type == TOKEN_LPAREN) {
        ASTNode *right = parse_factor(l, err);
        if (*err != REGEX_OK) {
            ast_free(left);
            return NULL;
        }
        left = ast_create_binary(AST_CONCAT, left, right);
        if (!left) { *err = REGEX_ERR_OUT_OF_MEMORY; return NULL; }
    }
    return left;
}

static ASTNode *parse_expr(Lexer *l, RegexError *err) {
    ASTNode *left = parse_term(l, err);
    if (*err != REGEX_OK) return NULL;

    while (l->current.type == TOKEN_ALTERNATE) {
        lexer_next(l);
        ASTNode *right = parse_term(l, err);
        if (*err != REGEX_OK) {
            ast_free(left);
            return NULL;
        }
        left = ast_create_binary(AST_ALTERNATION, left, right);
        if (!left) { *err = REGEX_ERR_OUT_OF_MEMORY; return NULL; }
    }
    return left;
}

void nfa_init(NFA *nfa) {
    nfa->state_count = 0;
    nfa->start_state = 0;
    nfa->accept_state = 0;
    memset(nfa->states, 0, sizeof(nfa->states));
}

int nfa_add_state(NFA *nfa, int edge_type, char c, int out1, int out2) {
    if (nfa->state_count >= MAX_STATES) return -1;
    int idx = nfa->state_count++;
    nfa->states[idx].edge_type = edge_type;
    nfa->states[idx].c = c;
    nfa->states[idx].out1 = out1;
    nfa->states[idx].out2 = out2;
    memset(nfa->states[idx].class_bits, 0, sizeof(nfa->states[idx].class_bits));
    return idx;
}

typedef struct {
    int start;
    int accept;
} Fragment;

static Fragment build_nfa_rec(const ASTNode *node, NFA *nfa) {
    Fragment frag;
    if (!node) {
        int s = nfa_add_state(nfa, 0, 0, -1, -1);
        frag.start = s;
        frag.accept = s;
        return frag;
    }

    switch (node->type) {
        case AST_LITERAL: {
            int s2 = nfa_add_state(nfa, 0, 0, -1, -1);
            int s1 = nfa_add_state(nfa, 1, node->literal, s2, -1);
            frag.start = s1;
            frag.accept = s2;
            break;
        }
        case AST_ANY: {
            int s2 = nfa_add_state(nfa, 0, 0, -1, -1);
            int s1 = nfa_add_state(nfa, 2, 0, s2, -1);
            frag.start = s1;
            frag.accept = s2;
            break;
        }
        case AST_CLASS: {
            int s2 = nfa_add_state(nfa, 0, 0, -1, -1);
            int s1 = nfa_add_state(nfa, 3, 0, s2, -1);
            memcpy(nfa->states[s1].class_bits, node->class_bits, sizeof(node->class_bits));
            if (node->negate_class) {
                // mark high bit or handle negation in transition check
            }
            frag.start = s1;
            frag.accept = s2;
            break;
        }
        case AST_CONCAT: {
            Fragment f1 = build_nfa_rec(node->left, nfa);
            Fragment f2 = build_nfa_rec(node->right, nfa);
            nfa->states[f1.accept].edge_type = 0;
            nfa->states[f1.accept].out1 = f2.start;
            frag.start = f1.start;
            frag.accept = f2.accept;
            break;
        }
        case AST_ALTERNATION: {
            Fragment f1 = build_nfa_rec(node->left, nfa);
            Fragment f2 = build_nfa_rec(node->right, nfa);
            int accept_state = nfa_add_state(nfa, 0, 0, -1, -1);
            int start_state = nfa_add_state(nfa, 0, 0, f1.start, f2.start);
            nfa->states[f1.accept].edge_type = 0;
            nfa->states[f1.accept].out1 = accept_state;
            nfa->states[f2.accept].edge_type = 0;
            nfa->states[f2.accept].out1 = accept_state;
            frag.start = start_state;
            frag.accept = accept_state;
            break;
        }
        case AST_STAR: {
            Fragment f1 = build_nfa_rec(node->left, nfa);
            int accept_state = nfa_add_state(nfa, 0, 0, -1, -1);
            int start_state = nfa_add_state(nfa, 0, 0, f1.start, accept_state);
            nfa->states[f1.accept].edge_type = 0;
            nfa->states[f1.accept].out1 = f1.start;
            nfa->states[f1.accept].out2 = accept_state;
            frag.start = start_state;
            frag.accept = accept_state;
            break;
        }
        case AST_PLUS: {
            Fragment f1 = build_nfa_rec(node->left, nfa);
            int accept_state = nfa_add_state(nfa, 0, 0, -1, -1);
            int start_state = nfa_add_state(nfa, 0, 0, f1.start, -1);
            nfa->states[f1.accept].edge_type = 0;
            nfa->states[f1.accept].out1 = f1.start;
            nfa->states[f1.accept].out2 = accept_state;
            frag.start = start_state;
            frag.accept = accept_state;
            break;
        }
        case AST_OPTIONAL: {
            Fragment f1 = build_nfa_rec(node->left, nfa);
            int accept_state = nfa_add_state(nfa, 0, 0, -1, -1);
            int start_state = nfa_add_state(nfa, 0, 0, f1.start, accept_state);
            nfa->states[f1.accept].edge_type = 0;
            nfa->states[f1.accept].out1 = accept_state;
            frag.start = start_state;
            frag.accept = accept_state;
            break;
        }
    }
    return frag;
}

static void epsilon_closure(const NFA *nfa, const bool *states_in, bool *states_out) {
    memcpy(states_out, states_in, MAX_STATES * sizeof(bool));
    int stack[MAX_STATES];
    int top = 0;

    for (int i = 0; i < nfa->state_count; ++i) {
        if (states_out[i]) {
            stack[top++] = i;
        }
    }

    while (top > 0) {
        int u = stack[--top];
        const NFAState *st = &nfa->states[u];
        if (st->edge_type == 0) {
            if (st->out1 >= 0 && !states_out[st->out1]) {
                states_out[st->out1] = true;
                stack[top++] = st->out1;
            }
            if (st->out2 >= 0 && !states_out[st->out2]) {
                states_out[st->out2] = true;
                stack[top++] = st->out2;
            }
        }
    }
}

static void move_transition(const NFA *nfa, const bool *current_states, char symbol, bool *next_states) {
    memset(next_states, 0, MAX_STATES * sizeof(bool));
    unsigned char sym = (unsigned char)symbol;

    for (int i = 0; i < nfa->state_count; ++i) {
        if (current_states[i]) {
            const NFAState *st = &nfa->states[i];
            if (st->edge_type == 1 && st->c == symbol) {
                if (st->out1 >= 0) next_states[st->out1] = true;
            } else if (st->edge_type == 2) {
                if (st->out1 >= 0) next_states[st->out1] = true;
            } else if (st->edge_type == 3) {
                bool in_class = (st->class_bits[sym / 8] & (1 << (sym % 8))) != 0;
                if (st->out1 >= 0 && in_class) next_states[st->out1] = true;
            }
        }
    }
}

int powerset_construction(const NFA *nfa, DFA *dfa) {
    memset(dfa, 0, sizeof(DFA));
    for (int i = 0; i < MAX_DFA_STATES; ++i) {
        for (int j = 0; j < ASCII_SIZE; ++j) {
            dfa->transitions[i][j] = -1;
        }
    }

    bool dfa_state_sets[MAX_DFA_STATES][MAX_STATES];
    memset(dfa_state_sets, 0, sizeof(dfa_state_sets));

    bool start_raw[MAX_STATES];
    memset(start_raw, 0, sizeof(start_raw));
    start_raw[nfa->start_state] = true;

    epsilon_closure(nfa, start_raw, dfa_state_sets[0]);
    dfa->state_count = 1;
    dfa->start_state = 0;

    int queue[MAX_DFA_STATES];
    int q_head = 0, q_tail = 0;
    queue[q_tail++] = 0;

    while (q_head < q_tail) {
        int current_dfa = queue[q_head++];

        for (int i = 0; i < MAX_STATES; ++i) {
            if (dfa_state_sets[current_dfa][i] && i == nfa->accept_state) {
                dfa->is_accept[current_dfa] = true;
            }
        }

        for (int sym = 0; sym < ASCII_SIZE; ++sym) {
            bool move_res[MAX_STATES];
            bool closure_res[MAX_STATES];
            move_transition(nfa, dfa_state_sets[current_dfa], (char)sym, move_res);
            epsilon_closure(nfa, move_res, closure_res);

            bool any_set = false;
            for (int i = 0; i < nfa->state_count; ++i) {
                if (closure_res[i]) { any_set = true; break; }
            }
            if (!any_set) continue;

            int existing_idx = -1;
            for (int d = 0; d < dfa->state_count; ++d) {
                bool match = true;
                for (int i = 0; i < nfa->state_count; ++i) {
                    if (dfa_state_sets[d][i] != closure_res[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    existing_idx = d;
                    break;
                }
            }

            if (existing_idx == -1) {
                if (dfa->state_count >= MAX_DFA_STATES) return -1;
                existing_idx = dfa->state_count++;
                memcpy(dfa_state_sets[existing_idx], closure_res, sizeof(closure_res));
                queue[q_tail++] = existing_idx;
            }

            dfa->transitions[current_dfa][sym] = existing_idx;
        }
    }
    return 0;
}

int minimize_dfa(DFA *dfa) {
    int partition[MAX_DFA_STATES];
    int p_count = 2;

    for (int i = 0; i < dfa->state_count; ++i) {
        partition[i] = dfa->is_accept[i] ? 1 : 0;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int sym = 0; sym < ASCII_SIZE; ++sym) {
            int next_p[MAX_DFA_STATES];
            for (int i = 0; i < dfa->state_count; ++i) {
                int target = dfa->transitions[i][sym];
                next_p[i] = (target >= 0) ? partition[target] : -1;
            }

            // refinement check
            int next_p_count = p_count;
            int new_partition_map[MAX_DFA_STATES];
            for (int i = 0; i < dfa->state_count; ++i) {
                new_partition_map[i] = partition[i];
            }

            for (int i = 0; i < dfa->state_count; ++i) {
                for (int j = i + 1; j < dfa->state_count; ++j) {
                    if (partition[i] == partition[j]) {
                        if (next_p[i] != next_p[j]) {
                            new_partition_map[j] = next_p_count;
                            changed = true;
                        }
                    }
                }
            }
            if (changed) {
                p_count++;
                for (int i = 0; i < dfa->state_count; ++i) {
                    partition[i] = new_partition_map[i];
                }
                break;
            }
        }
    }
    return p_count;
}

RegexError regex_compile(const char *pattern, RegexProgram *prog) {
    if (!pattern || !prog) return REGEX_ERR_COMPILE_FAILED;
    memset(prog, 0, sizeof(RegexProgram));

    if (strlen(pattern) == 0) {
        prog->error = REGEX_ERR_EMPTY_EXPRESSION;
        return prog->error;
    }

    Lexer lexer;
    lexer_init(&lexer, pattern);
    lexer_next(&lexer);

    RegexError err = REGEX_OK;
    ASTNode *ast = parse_expr(&lexer, &err);
    if (err != REGEX_OK || !ast) {
        prog->error = err;
        return err;
    }

    NFA nfa;
    nfa_init(&nfa);
    Fragment frag = build_nfa_rec(ast, &nfa);
    nfa.start_state = frag.start;
    nfa.accept_state = nfa_add_state(&nfa, 0, 0, -1, -1);
    nfa.states[frag.accept].edge_type = 0;
    nfa.states[frag.accept].out1 = nfa.accept_state;

    ast_free(ast);

    if (powerset_construction(&nfa, &prog->dfa) != 0) {
        prog->error = REGEX_ERR_DFA_OVERFLOW;
        return prog->error;
    }

    minimize_dfa(&prog->dfa);
    prog->error = REGEX_OK;
    return REGEX_OK;
}

void regex_free(RegexProgram *prog) {
    if (prog) {
        memset(prog, 0, sizeof(RegexProgram));
    }
}

bool regex_match(const RegexProgram *prog, const char *text) {
    size_t len = 0;
    return regex_match_ex(prog, text, &len);
}

bool regex_match_ex(const RegexProgram *prog, const char *text, size_t *match_len) {
    if (!prog || !text) return false;
    int current = prog->dfa.start_state;
    size_t last_accept_len = 0;
    bool matched = false;

    if (prog->dfa.is_accept[current]) {
        matched = true;
        last_accept_len = 0;
    }

    for (size_t i = 0; text[i] != '\0'; ++i) {
        unsigned char sym = (unsigned char)text[i];
        int next = prog->dfa.transitions[current][sym];
        if (next < 0) {
            break;
        }
        current = next;
        if (prog->dfa.is_accept[current]) {
            matched = true;
            last_accept_len = i + 1;
        }
    }

    if (match_len) {
        *match_len = last_accept_len;
    }
    return matched && (last_accept_len == strlen(text));
}
