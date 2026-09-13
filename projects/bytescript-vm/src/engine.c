#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define GC_HEAP_GROW_FACTOR 2

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_RETURN
} OpCode;

// --- Lexer ---
typedef enum {
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_AND, TOKEN_ELSE, TOKEN_FALSE, TOKEN_FOR,
    TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN, TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

static Lexer lexer;

static void initLexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
}

static bool isAtEnd(void) {
    return *lexer.current == '\0';
}

static char advance(void) {
    lexer.current++;
    return lexer.current[-1];
}

static char peek(void) {
    return *lexer.current;
}

static char peekNext(void) {
    if (isAtEnd()) return '\0';
    return lexer.current[1];
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*lexer.current != expected) return false;
    lexer.current++;
    return true;
}

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer.start;
    token.length = (int)(lexer.current - lexer.start);
    token.line = lexer.line;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    return token;
}

static void skipWhitespace(void) {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                lexer.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static Token identifier(void) {
    while (isAlpha(peek()) || isDigit(peek())) advance();

    // Check keywords
    int len = (int)(lexer.current - lexer.start);
    const char* s = lexer.start;
    TokenType type = TOKEN_IDENTIFIER;

    if (len == 3 && memcmp(s, "and", 3) == 0) type = TOKEN_AND;
    else if (len == 4 && memcmp(s, "else", 4) == 0) type = TOKEN_ELSE;
    else if (len == 5 && memcmp(s, "false", 5) == 0) type = TOKEN_FALSE;
    else if (len == 3 && memcmp(s, "for", 3) == 0) type = TOKEN_FOR;
    else if (len == 3 && memcmp(s, "fun", 3) == 0) type = TOKEN_FUN;
    else if (len == 2 && memcmp(s, "if", 2) == 0) type = TOKEN_IF;
    else if (len == 3 && memcmp(s, "nil", 3) == 0) type = TOKEN_NIL;
    else if (len == 2 && memcmp(s, "or", 2) == 0) type = TOKEN_OR;
    else if (len == 5 && memcmp(s, "print", 5) == 0) type = TOKEN_PRINT;
    else if (len == 6 && memcmp(s, "return", 6) == 0) type = TOKEN_RETURN;
    else if (len == 4 && memcmp(s, "true", 4) == 0) type = TOKEN_TRUE;
    else if (len == 3 && memcmp(s, "var", 3) == 0) type = TOKEN_VAR;
    else if (len == 5 && memcmp(s, "while", 5) == 0) type = TOKEN_WHILE;

    return makeToken(type);
}

static Token number(void) {
    while (isDigit(peek())) advance();
    if (peek() == '.' && isDigit(peekNext())) {
        advance();
        while (isDigit(peek())) advance();
    }
    return makeToken(TOKEN_NUMBER);
}

static Token string(void) {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') lexer.line++;
        advance();
    }
    if (isAtEnd()) return errorToken("Unterminated string.");
    advance(); // Closing quote
    return makeToken(TOKEN_STRING);
}

static Token scanToken(void) {
    skipWhitespace();
    lexer.start = lexer.current;
    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return string();
    }

    return errorToken("Unexpected character.");
}

// --- Chunk Operations ---
static void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->constants_count = 0;
    chunk->constants_capacity = 0;
    chunk->constants = NULL;
}

static void freeChunk(VM* vm, Chunk* chunk) {
    reallocate(vm, chunk->code, chunk->capacity * sizeof(uint8_t), 0);
    reallocate(vm, chunk->lines, chunk->capacity * sizeof(int), 0);
    for (int i = 0; i < chunk->constants_count; i++) {
        // Constants are values, GC will clean up objects
    }
    reallocate(vm, chunk->constants, chunk->constants_capacity * sizeof(Value), 0);
    initChunk(chunk);
}

static void writeChunk(VM* vm, Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->code = reallocate(vm, chunk->code, old_capacity * sizeof(uint8_t), chunk->capacity * sizeof(uint8_t));
        chunk->lines = reallocate(vm, chunk->lines, old_capacity * sizeof(int), chunk->capacity * sizeof(int));
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

static int addConstant(VM* vm, Chunk* chunk, Value value) {
    push(vm, value); // Protect from GC
    if (chunk->constants_capacity < chunk->constants_count + 1) {
        int old_capacity = chunk->constants_capacity;
        chunk->constants_capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->constants = reallocate(vm, chunk->constants, old_capacity * sizeof(Value), chunk->constants_capacity * sizeof(Value));
    }
    chunk->constants[chunk->constants_count] = value;
    chunk->constants_count++;
    pop(vm);
    return chunk->constants_count - 1;
}

// --- Memory & GC ---
void* reallocate(VM* vm, void* pointer, size_t old_size, size_t new_size) {
    vm->bytes_allocated += new_size - old_size;
    if (new_size > old_size) {
        if (vm->gc_enabled && vm->bytes_allocated > vm->next_gc) {
            collectGarbage(vm);
        }
    }
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }
    void* result = realloc(pointer, new_size);
    if (result == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    return result;
}

static void freeObject(VM* vm, Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)obj;
            reallocate(vm, string->chars, string->length + 1, 0);
            reallocate(vm, string, sizeof(ObjString), 0);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)obj;
            freeChunk(vm, &function->chunk);
            reallocate(vm, function, sizeof(ObjFunction), 0);
            break;
        }
        case OBJ_NATIVE: {
            reallocate(vm, obj, sizeof(ObjNative), 0);
            break;
        }
    }
}

void freeObjects(VM* vm) {
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }
    vm->objects = NULL;
}

// --- Table (Hash Map) ---
void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(VM* vm, Table* table) {
    reallocate(vm, table->entries, table->capacity * sizeof(TableEntry), 0);
    initTable(table);
}

static TableEntry* findEntry(TableEntry* entries, int capacity, ObjString* key) {
    uint32_t hash = 0;
    for (int i = 0; i < key->length; i++) {
        hash = (hash * 37) + key->chars[i];
    }
    uint32_t index = hash % capacity;
    TableEntry* tombstone = NULL;

    for (;;) {
        TableEntry* entry = &entries[index];
        if (entry->key == NULL) {
            if (is_nil(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

static void adjustCapacity(VM* vm, Table* table, int capacity) {
    TableEntry* entries = reallocate(vm, NULL, 0, capacity * sizeof(TableEntry));
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = val_nil();
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = &table->entries[i];
        if (entry->key == NULL) continue;
        TableEntry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    reallocate(vm, table->entries, table->capacity * sizeof(TableEntry), 0);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(VM* vm, Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * 0.75) {
        int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
        adjustCapacity(vm, table, capacity);
    }
    TableEntry* entry = findEntry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && is_nil(entry->value)) table->count++;
    entry->key = key;
    entry->value = value;
    return is_new_key;
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;
    TableEntry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;
    *value = entry->value;
    return true;
}

bool tableDelete(Table* table, ObjString* key) {
    if (table->count == 0) return false;
    TableEntry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;
    entry->key = NULL;
    entry->value = val_bool(true); // Tombstone
    return true;
}

// --- Object Allocation ---
static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
    object->type = type;
    object->is_marked = false;
    object->next = vm->objects;
    vm->objects = object;
    return object;
}

ObjString* allocateString(VM* vm, char* chars, int length) {
    ObjString* string = (ObjString*)allocateObject(vm, sizeof(ObjString), OBJ_STRING);
    string->length = length;
    string->chars = chars;
    push(vm, val_obj((Obj*)string));
    tableSet(vm, &vm->strings, string, val_nil());
    pop(vm);
    return string;
}

ObjString* copyString(VM* vm, const char* chars, int length) {
    // Check if string is already interned
    uint32_t hash = 0;
    for (int i = 0; i < length; i++) {
        hash = (hash * 37) + chars[i];
    }
    if (vm->strings.capacity > 0) {
        uint32_t index = hash % vm->strings.capacity;
        for (;;) {
            TableEntry* entry = &vm->strings.entries[index];
            if (entry->key == NULL) {
                if (is_nil(entry->value)) break;
            } else if (entry->key->length == length && memcmp(entry->key->chars, chars, length) == 0) {
                return entry->key;
            }
            index = (index + 1) % vm->strings.capacity;
        }
    }

    char* heap_chars = reallocate(vm, NULL, 0, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocateString(vm, heap_chars, length);
}

ObjString* takeString(VM* vm, char* chars, int length) {
    return allocateString(vm, chars, length);
}

ObjFunction* newFunction(VM* vm) {
    ObjFunction* function = (ObjFunction*)allocateObject(vm, sizeof(ObjFunction), OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(VM* vm, NativeFn function) {
    ObjNative* native = (ObjNative*)allocateObject(vm, sizeof(ObjNative), OBJ_NATIVE);
    native->function = function;
    return native;
}

// --- Garbage Collector Implementation ---
static void markObject(VM* vm, Obj* obj) {
    if (obj == NULL) return;
    if (obj->is_marked) return;
    obj->is_marked = true;

    // Trace references inside the object
    switch (obj->type) {
        case OBJ_STRING:
        case OBJ_NATIVE:
            break;
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)obj;
            markObject(vm, (Obj*)function->name);
            for (int i = 0; i < function->chunk.constants_count; i++) {
                if (is_obj(function->chunk.constants[i])) {
                    markObject(vm, function->chunk.constants[i].as.obj);
                }
            }
            break;
        }
    }
}

static void markValue(VM* vm, Value value) {
    if (is_obj(value)) {
        markObject(vm, value.as.obj);
    }
}

static void markRoots(VM* vm) {
    // Stack roots
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
        markValue(vm, *slot);
    }

    // Call frame roots
    for (int i = 0; i < vm->frame_count; i++) {
        markObject(vm, (Obj*)vm->frames[i].function);
    }

    // Global roots
    for (int i = 0; i < vm->globals.capacity; i++) {
        TableEntry* entry = &vm->globals.entries[i];
        if (entry->key != NULL) {
            markObject(vm, (Obj*)entry->key);
            markValue(vm, entry->value);
        }
    }
}

static void removeWhiteStrings(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            tableDelete(table, entry->key);
        }
    }
}

static void sweep(VM* vm) {
    Obj* previous = NULL;
    Obj* object = vm->objects;
    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm->objects = object;
            }
            freeObject(vm, unreached);
        }
    }
}

void collectGarbage(VM* vm) {
    size_t before = vm->bytes_allocated;
    markRoots(vm);
    removeWhiteStrings(&vm->strings);
    sweep(vm);
    vm->next_gc = vm->bytes_allocated * GC_HEAP_GROW_FACTOR;
    (void)before; // Avoid unused variable warning
}

// --- Compiler (Pratt Parser) ---
typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // ()
    PREC_PRIMARY
} Precedence;

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct CompileContext {
    struct CompileContext* enclosing;
    ObjFunction* function;
    Local locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
} CompileContext;

static Parser parser;
static VM* current_vm;
static CompileContext* current_compiler = NULL;

static Chunk* currentChunk(void) {
    return &current_compiler->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advanceParser(void) {
    parser.previous = parser.current;
    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advanceParser();
        return;
    }
    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool matchParser(TokenType type) {
    if (!check(type)) return false;
    advanceParser();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(current_vm, currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(int loop_start) {
    emitByte(OP_LOOP);
    int offset = currentChunk()->count - loop_start + 2;
    if (offset > 65535) error("Loop body too large.");
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn(void) {
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(current_vm, currentChunk(), value);
    if (constant > 255) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
    int jump = currentChunk()->count - offset - 2;
    if (jump > 65535) {
        error("Too many instructions to jump over.");
    }
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(CompileContext* context, CompileContext* enclosing) {
    context->enclosing = enclosing;
    context->function = NULL;
    context->local_count = 0;
    context->scope_depth = 0;
    context->function = newFunction(current_vm);
    current_compiler = context;

    if (enclosing != NULL) {
        context->function->name = copyString(current_vm, parser.previous.start, parser.previous.length);
    }

    Local* local = &context->locals[context->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction* endCompiler(void) {
    emitReturn();
    ObjFunction* function = current_compiler->function;
    current_compiler = current_compiler->enclosing;
    return function;
}

static void beginScope(void) {
    current_compiler->scope_depth++;
}

static void endScope(void) {
    current_compiler->scope_depth--;
    while (current_compiler->local_count > 0 &&
           current_compiler->locals[current_compiler->local_count - 1].depth > current_compiler->scope_depth) {
        emitByte(OP_POP);
        current_compiler->local_count--;
    }
}

// Forward declarations for Pratt parser
static void expression(void);
static void statement(void);
static void declaration(void);

typedef void (*ParseFn)(bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void binary(bool can_assign) {
    (void)can_assign;
    TokenType operator_type = parser.previous.type;
    ParseRule* rule = getRule(operator_type);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operator_type) {
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:       emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:     emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emitByte(OP_ADD); break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
        default: return;
    }
}

static void call(bool can_assign) {
    (void)can_assign;
    uint8_t arg_count = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (arg_count == 255) {
                error("Can't have more than 255 arguments.");
            }
            arg_count++;
        } while (matchParser(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    emitBytes(OP_CALL, arg_count);
}

static void literal(bool can_assign) {
    (void)can_assign;
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return;
    }
}

static void grouping(bool can_assign) {
    (void)can_assign;
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number_parse(bool can_assign) {
    (void)can_assign;
    double value = strtod(parser.previous.start, NULL);
    emitConstant(val_number(value));
}

static void string_parse(bool can_assign) {
    (void)can_assign;
    emitConstant(val_obj((Obj*)copyString(current_vm, parser.previous.start + 1, parser.previous.length - 2)));
}

static int resolveLocal(CompileContext* compiler, Token* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->name.length == name->length &&
            memcmp(local->name.start, name->start, name->length) == 0) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static void namedVariable(Token name, bool can_assign) {
    uint8_t get_op, set_op;
    int arg = resolveLocal(current_compiler, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else {
        arg = makeConstant(val_obj((Obj*)copyString(current_vm, name.start, name.length)));
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && matchParser(TOKEN_EQUAL)) {
        expression();
        emitBytes(set_op, (uint8_t)arg);
    } else {
        emitBytes(get_op, (uint8_t)arg);
    }
}

static void variable(bool can_assign) {
    namedVariable(parser.previous, can_assign);
}

static void unary(bool can_assign) {
    (void)can_assign;
    TokenType operator_type = parser.previous.type;
    parsePrecedence(PREC_UNARY);
    switch (operator_type) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return;
    }
}

static void and_(bool can_assign) {
    (void)can_assign;
    int end_jump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(end_jump);
}

static void or_(bool can_assign) {
    (void)can_assign;
    int else_jump = emitJump(OP_JUMP_IF_FALSE);
    int end_jump = emitJump(OP_JUMP);
    patchJump(else_jump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(end_jump);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string_parse, NULL, PREC_NONE},
    [TOKEN_NUMBER]        = {number_parse, NULL, PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void parsePrecedence(Precedence precedence) {
    advanceParser();
    ParseFn prefix_rule = getRule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advanceParser();
        ParseFn infix_rule = getRule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && matchParser(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static void expression(void) {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block(void) {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(void) {
    CompileContext compiler;
    initCompiler(&compiler, current_compiler);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current_compiler->function->arity++;
            if (current_compiler->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            consume(TOKEN_IDENTIFIER, "Expect parameter name.");
            // Declare parameter as local variable
            Token name = parser.previous;
            for (int i = current_compiler->local_count - 1; i >= 0; i--) {
                Local* local = &current_compiler->locals[i];
                if (local->depth != -1 && local->depth < current_compiler->scope_depth) {
                    break;
                }
                if (local->name.length == name.length &&
                    memcmp(local->name.start, name.start, name.length) == 0) {
                    error("Already a variable with this name in this scope.");
                }
            }
            Local* local = &current_compiler->locals[current_compiler->local_count++];
            local->name = name;
            local->depth = current_compiler->scope_depth;
        } while (matchParser(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* compiled_fn = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(val_obj((Obj*)compiled_fn)));
}

static void funDeclaration(void) {
    consume(TOKEN_IDENTIFIER, "Expect function name.");
    Token name = parser.previous;
    uint8_t global_constant = 0;
    if (current_compiler->scope_depth == 0) {
        global_constant = makeConstant(val_obj((Obj*)copyString(current_vm, name.start, name.length)));
    } else {
        // Local function declaration
        for (int i = current_compiler->local_count - 1; i >= 0; i--) {
            Local* local = &current_compiler->locals[i];
            if (local->depth != -1 && local->depth < current_compiler->scope_depth) {
                break;
            }
            if (local->name.length == name.length &&
                memcmp(local->name.start, name.start, name.length) == 0) {
                error("Already a variable with this name in this scope.");
            }
        }
        Local* local = &current_compiler->locals[current_compiler->local_count++];
        local->name = name;
        local->depth = -1;
    }

    function();

    if (current_compiler->scope_depth == 0) {
        emitBytes(OP_SET_GLOBAL, global_constant);
        emitByte(OP_POP);
    } else {
        current_compiler->locals[current_compiler->local_count - 1].depth = current_compiler->scope_depth;
    }
}

static void varDeclaration(void) {
    consume(TOKEN_IDENTIFIER, "Expect variable name.");
    Token name = parser.previous;
    uint8_t global_constant = 0;
    if (current_compiler->scope_depth == 0) {
        global_constant = makeConstant(val_obj((Obj*)copyString(current_vm, name.start, name.length)));
    } else {
        for (int i = current_compiler->local_count - 1; i >= 0; i--) {
            Local* local = &current_compiler->locals[i];
            if (local->depth != -1 && local->depth < current_compiler->scope_depth) {
                break;
            }
            if (local->name.length == name.length &&
                memcmp(local->name.start, name.start, name.length) == 0) {
                error("Already a variable with this name in this scope.");
            }
        }
        Local* local = &current_compiler->locals[current_compiler->local_count++];
        local->name = name;
        local->depth = -1;
    }

    if (matchParser(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    if (current_compiler->scope_depth == 0) {
        emitBytes(OP_SET_GLOBAL, global_constant);
        emitByte(OP_POP);
    } else {
        current_compiler->locals[current_compiler->local_count - 1].depth = current_compiler->scope_depth;
    }
}

static void printStatement(void) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void returnStatement(void) {
    if (current_compiler->enclosing == NULL) {
        error("Can't return from top-level code.");
    }
    if (matchParser(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void expressionStatement(void) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void ifStatement(void) {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int then_jump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int else_jump = emitJump(OP_JUMP);
    patchJump(then_jump);
    emitByte(OP_POP);

    if (matchParser(TOKEN_ELSE)) statement();
    patchJump(else_jump);
}

static void whileStatement(void) {
    int loop_start = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loop_start);

    patchJump(exit_jump);
    emitByte(OP_POP);
}

static void declaration(void) {
    if (matchParser(TOKEN_FUN)) {
        funDeclaration();
    } else if (matchParser(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panic_mode) {
        // Synchronize parser
        parser.panic_mode = false;
        while (parser.current.type != TOKEN_EOF) {
            if (parser.previous.type == TOKEN_SEMICOLON) return;
            switch (parser.current.type) {
                case TOKEN_FUN:
                case TOKEN_VAR:
                case TOKEN_IF:
                case TOKEN_WHILE:
                case TOKEN_PRINT:
                case TOKEN_RETURN:
                    return;
                default:
                    ;
            }
            advanceParser();
        }
    }
}

static void statement(void) {
    if (matchParser(TOKEN_PRINT)) {
        printStatement();
    } else if (matchParser(TOKEN_IF)) {
        ifStatement();
    } else if (matchParser(TOKEN_RETURN)) {
        returnStatement();
    } else if (matchParser(TOKEN_WHILE)) {
        whileStatement();
    } else if (matchParser(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

// --- VM Implementation ---
void initVM(VM* vm) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 1024;
    vm->gc_enabled = true;
    initTable(&vm->globals);
    initTable(&vm->strings);
}

void freeVM(VM* vm) {
    freeTable(vm, &vm->globals);
    freeTable(vm, &vm->strings);
    freeObjects(vm);
}

void push(VM* vm, Value value) {
    if (vm->stack_top >= vm->stack + STACK_MAX) {
        fprintf(stderr, "Stack overflow.\n");
        exit(1);
    }
    *vm->stack_top = value;
    vm->stack_top++;
}

Value pop(VM* vm) {
    if (vm->stack_top == vm->stack) {
        fprintf(stderr, "Stack underflow.\n");
        exit(1);
    }
    vm->stack_top--;
    return *vm->stack_top;
}

static Value peekVM(VM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

static bool callValue(VM* vm, Value callee, int arg_count) {
    if (is_obj(callee)) {
        switch (callee.as.obj->type) {
            case OBJ_FUNCTION: {
                ObjFunction* function = (ObjFunction*)callee.as.obj;
                if (arg_count != function->arity) {
                    fprintf(stderr, "Expected %d arguments but got %d.\n", function->arity, arg_count);
                    return false;
                }
                if (vm->frame_count >= FRAMES_MAX) {
                    fprintf(stderr, "Stack overflow (too many call frames).\n");
                    return false;
                }
                CallFrame* frame = &vm->frames[vm->frame_count++];
                frame->function = function;
                frame->ip = function->chunk.code;
                frame->slots = vm->stack_top - arg_count - 1;
                return true;
            }
            case OBJ_NATIVE: {
                ObjNative* native = (ObjNative*)callee.as.obj;
                Value result = native->function(arg_count, vm->stack_top - arg_count);
                vm->stack_top -= arg_count + 1;
                push(vm, result);
                return true;
            }
            default:
                break;
        }
    }
    fprintf(stderr, "Can only call functions and natives.\n");
    return false;
}

static bool isFalsey(Value value) {
    return is_nil(value) || (is_bool(value) && !value.as.boolean);
}

static bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_BOOL: return a.as.boolean == b.as.boolean;
        case VAL_NUMBER: return a.as.number == b.as.number;
        case VAL_OBJ: return a.as.obj == b.as.obj;
    }
    return false;
}

InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants[READ_BYTE()])
#define READ_STRING() ((ObjString*)READ_CONSTANT().as.obj)

    for (;;) {
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_NIL: push(vm, val_nil()); break;
            case OP_TRUE: push(vm, val_bool(true)); break;
            case OP_FALSE: push(vm, val_bool(false)); break;
            case OP_POP: pop(vm); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peekVM(vm, 0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm->globals, name, &value)) {
                    fprintf(stderr, "Undefined variable '%s'.\n", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(vm, &vm->globals, name, peekVM(vm, 0));
                break;
            }
            case OP_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, val_bool(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: {
                if (!is_number(peekVM(vm, 0)) || !is_number(peekVM(vm, 1))) {
                    fprintf(stderr, "Operands must be numbers.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, val_bool(a.as.number > b.as.number));
                break;
            }
            case OP_LESS: {
                if (!is_number(peekVM(vm, 0)) || !is_number(peekVM(vm, 1))) {
                    fprintf(stderr, "Operands must be numbers.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, val_bool(a.as.number < b.as.number));
                break;
            }
            case OP_ADD: {
                if (is_number(peekVM(vm, 0)) && is_number(peekVM(vm, 1))) {
                    Value b = pop(vm);
                    Value a = pop(vm);
                    push(vm, val_number(a.as.number + b.as.number));
                } else if (is_obj_type(peekVM(vm, 0), OBJ_STRING) && is_obj_type(peekVM(vm, 1), OBJ_STRING)) {
                    ObjString* b = (ObjString*)pop(vm).as.obj;
                    ObjString* a = (ObjString*)pop(vm).as.obj;
                    int length = a->length + b->length;
                    char* chars = reallocate(vm, NULL, 0, length + 1);
                    memcpy(chars, a->chars, a->length);
                    memcpy(chars + a->length, b->chars, b->length);
                    chars[length] = '\0';
                    ObjString* result = takeString(vm, chars, length);
                    push(vm, val_obj((Obj*)result));
                } else {
                    fprintf(stderr, "Operands must be two numbers or two strings.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: {
                if (!is_number(peekVM(vm, 0)) || !is_number(peekVM(vm, 1))) {
                    fprintf(stderr, "Operands must be numbers.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, val_number(a.as.number - b.as.number));
                break;
            }
            case OP_MULTIPLY: {
                if (!is_number(peekVM(vm, 0)) || !is_number(peekVM(vm, 1))) {
                    fprintf(stderr, "Operands must be numbers.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, val_number(a.as.number * b.as.number));
                break;
            }
            case OP_DIVIDE: {
                if (!is_number(peekVM(vm, 0)) || !is_number(peekVM(vm, 1))) {
                    fprintf(stderr, "Operands must be numbers.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, val_number(a.as.number / b.as.number));
                break;
            }
            case OP_NOT: {
                Value val = pop(vm);
                push(vm, val_bool(isFalsey(val)));
                break;
            }
            case OP_NEGATE: {
                if (!is_number(peekVM(vm, 0))) {
                    fprintf(stderr, "Operand must be a number.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value val = pop(vm);
                push(vm, val_number(-val.as.number));
                break;
            }
            case OP_PRINT: {
                Value val = pop(vm);
                if (is_nil(val)) printf("nil\n");
                else if (is_bool(val)) printf(val.as.boolean ? "true\n" : "false\n");
                else if (is_number(val)) printf("%g\n", val.as.number);
                else if (is_obj_type(val, OBJ_STRING)) printf("%s\n", ((ObjString*)val.as.obj)->chars);
                else if (is_obj_type(val, OBJ_FUNCTION)) printf("<fn %s>\n", ((ObjFunction*)val.as.obj)->name ? ((ObjFunction*)val.as.obj)->name->chars : "anonymous");
                else if (is_obj_type(val, OBJ_NATIVE)) printf("<native fn>\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peekVM(vm, 0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int arg_count = READ_BYTE();
                if (!callValue(vm, peekVM(vm, arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            case OP_RETURN: {
                Value result = pop(vm);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }
                vm->stack_top = frame->slots;
                push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
}

InterpretResult interpret(VM* vm, const char* source) {
    initLexer(source);
    CompileContext context;
    current_vm = vm;
    initCompiler(&context, NULL);

    parser.had_error = false;
    parser.panic_mode = false;
    advanceParser();

    while (!matchParser(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = endCompiler();
    if (parser.had_error) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(vm, val_obj((Obj*)function));
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm->stack_top - 1;

    return run(vm);
}

// --- Disassembler Implementation ---
void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    Value val = chunk->constants[constant];
    if (is_nil(val)) printf("nil");
    else if (is_bool(val)) printf(val.as.boolean ? "true" : "false");
    else if (is_number(val)) printf("%g", val.as.number);
    else if (is_obj_type(val, OBJ_STRING)) printf("%s", ((ObjString*)val.as.obj)->chars);
    printf("'\n");
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT: return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NIL: return simpleInstruction("OP_NIL", offset);
        case OP_TRUE: return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE: return simpleInstruction("OP_FALSE", offset);
        case OP_POP: return simpleInstruction("OP_POP", offset);
        case OP_GET_LOCAL: return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL: return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL: return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL: return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_EQUAL: return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER: return simpleInstruction("OP_GREATER", offset);
        case OP_LESS: return simpleInstruction("OP_LESS", offset);
        case OP_ADD: return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT: return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY: return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE: return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT: return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE: return simpleInstruction("OP_NEGATE", offset);
        case OP_PRINT: return simpleInstruction("OP_PRINT", offset);
        case OP_JUMP: return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP: return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL: return byteInstruction("OP_CALL", chunk, offset);
        case OP_RETURN: return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
