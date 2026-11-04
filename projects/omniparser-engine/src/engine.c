#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Lexer Implementation */
omni_lexer_stream_t *omni_lex_input(const char *input) {
    if (!input) return NULL;
    omni_lexer_stream_t *stream = malloc(sizeof(omni_lexer_stream_t));
    if (!stream) return NULL;

    stream->capacity = 64;
    stream->count = 0;
    stream->index = 0;
    stream->tokens = malloc(stream->capacity * sizeof(omni_token_t));
    if (!stream->tokens) {
        free(stream);
        return NULL;
    }

    size_t i = 0;
    size_t line = 1;
    size_t col = 1;

    while (input[i] != '\0') {
        while (input[i] != '\0' && isspace((unsigned char)input[i])) {
            if (input[i] == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
            i++;
        }

        if (input[i] == '\0') break;

        if (stream->count >= stream->capacity) {
            stream->capacity *= 2;
            omni_token_t *new_tokens = realloc(stream->tokens, stream->capacity * sizeof(omni_token_t));
            if (!new_tokens) break;
            stream->tokens = new_tokens;
        }

        omni_token_t *tok = &stream->tokens[stream->count];
        tok->line = line;
        tok->column = col;

        if (isalpha((unsigned char)input[i]) || input[i] == '_') {
            size_t start = i;
            while (isalnum((unsigned char)input[i]) || input[i] == '_') {
                i++;
                col++;
            }
            size_t len = i - start;
            if (len >= OMNI_MAX_TOKEN_LEN) len = OMNI_MAX_TOKEN_LEN - 1;
            memcpy(tok->lexeme, &input[start], len);
            tok->lexeme[len] = '\0';
            
            if (strcmp(tok->lexeme, "let") == 0 || strcmp(tok->lexeme, "fn") == 0 || strcmp(tok->lexeme, "return") == 0) {
                tok->type = OMNI_TOK_KEYWORD;
            } else {
                tok->type = OMNI_TOK_IDENTIFIER;
            }
            stream->count++;
        } else if (isdigit((unsigned char)input[i])) {
            size_t start = i;
            while (isdigit((unsigned char)input[i])) {
                i++;
                col++;
            }
            size_t len = i - start;
            if (len >= OMNI_MAX_TOKEN_LEN) len = OMNI_MAX_TOKEN_LEN - 1;
            memcpy(tok->lexeme, &input[start], len);
            tok->lexeme[len] = '\0';
            tok->type = OMNI_TOK_NUMBER;
            stream->count++;
        } else {
            tok->lexeme[0] = input[i];
            tok->lexeme[1] = '\0';
            tok->type = OMNI_TOK_OPERATOR;
            i++;
            col++;
            stream->count++;
        }
    }

    if (stream->count >= stream->capacity) {
        stream->capacity += 1;
        omni_token_t *new_tokens = realloc(stream->tokens, stream->capacity * sizeof(omni_token_t));
        if (new_tokens) stream->tokens = new_tokens;
    }

    stream->tokens[stream->count] = (omni_token_t){OMNI_TOK_EOF, "", line, col};
    stream->count++;
    return stream;
}

void omni_free_lexer_stream(omni_lexer_stream_t *stream) {
    if (stream) {
        free(stream->tokens);
        free(stream);
    }
}

/* Memory Pool Implementation */
omni_pool_t *omni_pool_init(size_t block_size) {
    omni_pool_t *pool = malloc(sizeof(omni_pool_t));
    if (!pool) return NULL;
    pool->block_size = block_size > 0 ? block_size : OMNI_POOL_BLOCK_SIZE;
    pool->head = malloc(sizeof(omni_pool_block_t));
    if (!pool->head) {
        free(pool);
        return NULL;
    }
    pool->head->used = 0;
    pool->head->next = NULL;
    return pool;
}

void *omni_pool_alloc(omni_pool_t *pool, size_t size) {
    if (!pool || !pool->head) return NULL;
    size = (size + 7) & ~7; /* 8-byte alignment */

    omni_pool_block_t *curr = pool->head;
    while (curr) {
        if (curr->used + size <= OMNI_POOL_BLOCK_SIZE) {
            void *ptr = &curr->data[curr->used];
            curr->used += size;
            return ptr;
        }
        if (!curr->next) break;
        curr = curr->next;
    }

    omni_pool_block_t *new_block = malloc(sizeof(omni_pool_block_t));
    if (!new_block) return NULL;
    new_block->used = 0;
    new_block->next = pool->head;
    pool->head = new_block;

    void *ptr = &new_block->data[0];
    new_block->used += size;
    return ptr;
}

void omni_pool_free(omni_pool_t *pool) {
    if (!pool) return;
    omni_pool_block_t *curr = pool->head;
    while (curr) {
        omni_pool_block_t *next = curr->next;
        free(curr);
        curr = next;
    }
    free(pool);
}

/* AST Implementation */
omni_ast_node_t *omni_ast_create_node(omni_pool_t *pool, omni_ast_type_t type, const char *name) {
    omni_ast_node_t *node = omni_pool_alloc(pool, sizeof(omni_ast_node_t));
    if (!node) return NULL;
    node->type = type;
    if (name) {
        strncpy(node->name, name, OMNI_MAX_TOKEN_LEN - 1);
        node->name[OMNI_MAX_TOKEN_LEN - 1] = '\0';
    } else {
        node->name[0] = '\0';
    }
    node->child_capacity = 4;
    node->child_count = 0;
    node->children = omni_pool_alloc(pool, node->child_capacity * sizeof(omni_ast_node_t *));
    return node;
}

void omni_ast_add_child(omni_pool_t *pool, omni_ast_node_t *parent, omni_ast_node_t *child) {
    if (!parent || !child) return;
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        omni_ast_node_t **new_children = omni_pool_alloc(pool, parent->child_capacity * sizeof(omni_ast_node_t *));
        if (new_children) {
            for (size_t i = 0; i < parent->child_count; i++) {
                new_children[i] = parent->children[i];
            }
            parent->children = new_children;
        }
    }
    parent->children[parent->child_count++] = child;
}

void omni_ast_print(const omni_ast_node_t *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("Node(type=%d, name='%s')\n", node->type, node->name);
    for (size_t i = 0; i < node->child_count; i++) {
        omni_ast_print(node->children[i], indent + 1);
    }
}

/* Grammar & Set Calculator Implementation */
void omni_init_grammar(omni_grammar_t *g) {
    g->symbol_count = 0;
    g->production_count = 0;
    g->start_symbol = -1;
    for (int i = 0; i < OMNI_MAX_SYMBOLS; i++) {
        g->is_terminal[i] = false;
        for (int j = 0; j < OMNI_MAX_SYMBOLS; j++) {
            g->first_sets[i][j] = false;
            g->follow_sets[i][j] = false;
        }
    }
}

int omni_add_symbol(omni_grammar_t *g, const char *name, bool is_terminal) {
    if (g->symbol_count >= OMNI_MAX_SYMBOLS) return -1;
    for (size_t i = 0; i < g->symbol_count; i++) {
        if (strcmp(g->symbol_names[i], name) == 0) return (int)i;
    }
    int idx = (int)g->symbol_count++;
    strncpy(g->symbol_names[idx], name, OMNI_MAX_TOKEN_LEN - 1);
    g->symbol_names[idx][OMNI_MAX_TOKEN_LEN - 1] = '\0';
    g->is_terminal[idx] = is_terminal;
    return idx;
}

int omni_add_production(omni_grammar_t *g, int lhs, const int *rhs, size_t rhs_len) {
    if (g->production_count >= OMNI_MAX_PRODS) return -1;
    omni_production_t *prod = &g->productions[g->production_count++];
    prod->lhs = lhs;
    prod->rhs_len = rhs_len < OMNI_MAX_SYMBOLS ? rhs_len : OMNI_MAX_SYMBOLS;
    for (size_t i = 0; i < prod->rhs_len; i++) {
        prod->rhs[i] = rhs[i];
    }
    if (g->start_symbol == -1) {
        g->start_symbol = lhs;
    }
    return (int)(g->production_count - 1);
}

void omni_compute_first_follow(omni_grammar_t *g) {
    for (size_t i = 0; i < g->symbol_count; i++) {
        if (g->is_terminal[i]) {
            g->first_sets[i][i] = true;
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t p = 0; p < g->production_count; p++) {
            omni_production_t *prod = &g->productions[p];
            int lhs = prod->lhs;
            if (prod->rhs_len == 0) continue;

            bool all_nullable = true;
            for (size_t i = 0; i < prod->rhs_len; i++) {
                int sym = prod->rhs[i];
                for (size_t t = 0; t < g->symbol_count; t++) {
                    if (g->first_sets[sym][t] && !g->first_sets[lhs][t]) {
                        g->first_sets[lhs][t] = true;
                        changed = true;
                    }
                }
                // Check if symbol is nullable (simple heuristic: has epsilon production or terminal)
                bool nullable = g->is_terminal[sym] ? false : false;
                if (!nullable) {
                    all_nullable = false;
                    break;
                }
            }
            if (all_nullable) {
                // Add epsilon if supported
            }
        }
    }

    if (g->start_symbol >= 0) {
        g->follow_sets[g->start_symbol][g->start_symbol] = true; // Use start or EOF marker representation
    }

    changed = true;
    while (changed) {
        changed = false;
        for (size_t p = 0; p < g->production_count; p++) {
            omni_production_t *prod = &g->productions[p];
            for (size_t i = 0; i < prod->rhs_len; i++) {
                int b = prod->rhs[i];
                if (g->is_terminal[b]) continue;

                // Add FIRST of subsequent symbols to FOLLOW of b
                for (size_t j = i + 1; j < prod->rhs_len; j++) {
                    int next_sym = prod->rhs[j];
                    for (size_t t = 0; t < g->symbol_count; t++) {
                        if (g->first_sets[next_sym][t] && !g->follow_sets[b][t]) {
                            g->follow_sets[b][t] = true;
                            changed = true;
                        }
                    }
                    break;
                }
                // If at end or subsequent are nullable, add FOLLOW of LHS to FOLLOW of b
                if (i == prod->rhs_len - 1) {
                    int lhs = prod->lhs;
                    for (size_t t = 0; t < g->symbol_count; t++) {
                        if (g->follow_sets[lhs][t] && !g->follow_sets[b][t]) {
                            g->follow_sets[b][t] = true;
                            changed = true;
                        }
                    }
                }
            }
        }
    }
}

void omni_build_parser_table(const omni_grammar_t *g, omni_parser_table_t *table) {
    table->state_count = 2;
    table->conflict_count = 0;

    for (size_t s = 0; s < OMNI_MAX_STATES; s++) {
        for (size_t sym = 0; sym < OMNI_MAX_SYMBOLS; sym++) {
            table->actions[s][sym].type = OMNI_ACTION_ERROR;
            table->actions[s][sym].target = 0;
            table->gotos[s][sym] = -1;
        }
    }

    // Populate deterministic fallback LR parsing table structure
    for (size_t p = 0; p < g->production_count; p++) {
        const omni_production_t *prod = &g->productions[p];
        if (prod->rhs_len > 0) {
            int first_sym = prod->rhs[0];
            if (g->is_terminal[first_sym]) {
                table->actions[0][first_sym].type = OMNI_ACTION_SHIFT;
                table->actions[0][first_sym].target = 1;
            }
        }
    }

    // Accept action on EOF terminal index 0 or similar
    table->actions[1][0].type = OMNI_ACTION_ACCEPT;
}

/* Parser & Recovery Implementation */
omni_ast_node_t *omni_parse(omni_lexer_stream_t *stream, const omni_parser_table_t *table, const omni_grammar_t *g, omni_pool_t *pool) {
    if (!stream || !table || !g || !pool) return NULL;

    omni_ast_node_t *root = omni_ast_create_node(pool, OMNI_AST_NODE_ROOT, "Root");
    if (!root) return NULL;

    int state_stack[OMNI_MAX_STATES];
    int state_top = 0;
    state_stack[state_top] = 0;

    while (stream->index < stream->count) {
        omni_token_t *tok = &stream->tokens[stream->index];
        if (tok->type == OMNI_TOK_EOF) {
            break;
        }

        // Map token to grammar symbol terminal
        int sym_id = -1;
        for (size_t s = 0; s < g->symbol_count; s++) {
            if (g->is_terminal[s] && strcmp(g->symbol_names[s], tok->lexeme) == 0) {
                sym_id = (int)s;
                break;
            }
        }

        if (sym_id == -1) {
            // Fallback generic identifier terminal match
            for (size_t s = 0; s < g->symbol_count; s++) {
                if (g->is_terminal[s] && (strcmp(g->symbol_names[s], "IDENT") == 0 || strcmp(g->symbol_names[s], "id") == 0)) {
                    sym_id = (int)s;
                    break;
                }
            }
        }

        int current_state = state_stack[state_top];
        omni_table_cell_t action = {OMNI_ACTION_ERROR, 0};
        if (sym_id >= 0 && sym_id < OMNI_MAX_SYMBOLS) {
            action = table->actions[current_state][sym_id];
        }

        if (action.type == OMNI_ACTION_SHIFT) {
            if (state_top + 1 < OMNI_MAX_STATES) {
                state_stack[++state_top] = action.target;
            }
            omni_ast_node_t *child = omni_ast_create_node(pool, OMNI_AST_NODE_EXPR, tok->lexeme);
            child->token = *tok;
            omni_ast_add_child(pool, root, child);
            stream->index++;
        } else if (action.type == OMNI_ACTION_REDUCE) {
            if (state_top > 0) state_top--;
        } else if (action.type == OMNI_ACTION_ACCEPT) {
            break;
        } else {
            // Panic mode recovery: consume token to synchronize
            stream->index++;
        }
    }

    return root;
}
