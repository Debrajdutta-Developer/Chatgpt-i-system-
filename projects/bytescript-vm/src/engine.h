#ifndef BYTESCRIPT_ENGINE_H
#define BYTESCRIPT_ENGINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define STACK_MAX 2048
#define FRAMES_MAX 64
#define UINT8_COUNT 256

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE
} ObjType;

struct Obj {
    ObjType type;
    bool is_marked;
    struct Obj* next;
};

typedef struct Obj Obj;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

typedef struct {
    Obj obj;
    int length;
    char* chars;
} ObjString;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    int constants_count;
    int constants_capacity;
    Value* constants;
} Chunk;

typedef struct {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef struct VM VM;
typedef Value (*NativeFn)(int arg_count, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    ObjString* key;
    Value value;
} TableEntry;

typedef struct {
    int count;
    int capacity;
    TableEntry* entries;
} Table;

struct VM {
    CallFrame frames[FRAMES_MAX];
    int frame_count;
    Value stack[STACK_MAX];
    Value* stack_top;
    Table globals;
    Table strings;
    Obj* objects;
    size_t bytes_allocated;
    size_t next_gc;
    bool gc_enabled;
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm, const char* source);
void push(VM* vm, Value value);
Value pop(VM* vm);

// Memory management & GC
void* reallocate(VM* vm, void* pointer, size_t old_size, size_t new_size);
void collectGarbage(VM* vm);
void freeObjects(VM* vm);

// Object creation
ObjString* copyString(VM* vm, const char* chars, int length);
ObjString* takeString(VM* vm, char* chars, int length);
ObjFunction* newFunction(VM* vm);
ObjNative* newNative(VM* vm, NativeFn function);

// Table operations
void initTable(Table* table);
void freeTable(VM* vm, Table* table);
bool tableSet(VM* vm, Table* table, ObjString* key, Value value);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableDelete(Table* table, ObjString* key);

// Disassembler
void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

// Value helpers
static inline Value val_nil(void) {
    Value val;
    val.type = VAL_NIL;
    val.as.number = 0;
    return val;
}

static inline Value val_bool(bool b) {
    Value val;
    val.type = VAL_BOOL;
    val.as.boolean = b;
    return val;
}

static inline Value val_number(double num) {
    Value val;
    val.type = VAL_NUMBER;
    val.as.number = num;
    return val;
}

static inline Value val_obj(Obj* obj) {
    Value val;
    val.type = VAL_OBJ;
    val.as.obj = obj;
    return val;
}

static inline bool is_nil(Value val) {
    return val.type == VAL_NIL;
}

static inline bool is_bool(Value val) {
    return val.type == VAL_BOOL;
}

static inline bool is_number(Value val) {
    return val.type == VAL_NUMBER;
}

static inline bool is_obj(Value val) {
    return val.type == VAL_OBJ;
}

static inline bool is_obj_type(Value val, ObjType type) {
    return is_obj(val) && val.as.obj->type == type;
}

#endif
