#ifndef OMNI_ENGINE_H
#define OMNI_ENGINE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    OBJ_NIL,
    OBJ_BOOL,
    OBJ_INT,
    OBJ_FLOAT,
    OBJ_SYMBOL,
    OBJ_STRING,
    OBJ_CONS,
    OBJ_PRIMITIVE,
    OBJ_CLOSURE,
    OBJ_MACRO,
    OBJ_EOF
} ObjType;

struct Obj;
struct Env;

typedef struct Obj* (*PrimitiveFn)(struct Env* env, struct Obj* args);

typedef struct Obj {
    ObjType type;
    bool marked;
    struct Obj* next;
    union {
        int64_t ival;
        double fval;
        bool bval;
        char* sval;
        struct {
            struct Obj* car;
            struct Obj* cdr;
        } cons;
        PrimitiveFn pval;
        struct {
            struct Obj* params;
            struct Obj* body;
            struct Env* env;
        } closure;
    } as;
} Obj;

typedef struct Binding {
    char* name;
    Obj* obj;
    struct Binding* next;
} Binding;

typedef struct Env {
    Binding* head;
    struct Env* parent;
}
Env;

typedef struct {
    const char* src;
    size_t pos;
    size_t len;
} Lexer;

/* Core Engine API */
void omni_init(void);
void omni_cleanup(void);

Obj* omni_nil(void);
Obj* omni_make_bool(bool val);
Obj* omni_make_int(int64_t val);
Obj* omni_make_float(double val);
Obj* omni_make_symbol(const char* name);
Obj* omni_make_string(const char* val);
Obj* omni_make_cons(Obj* car, Obj* cdr);
Obj* omni_make_primitive(PrimitiveFn fn);
Obj* omni_make_closure(Obj* params, Obj* body, Env* env);
Obj* omni_make_macro(Obj* params, Obj* body, Env* env);

Env* omni_env_create(Env* parent);
void omni_env_bind(Env* env, const char* name, Obj* obj);
Obj* omni_env_lookup(Env* env, const char* name);

Obj* omni_read(const char* input);
Obj* omni_eval(Obj* expr, Env* env);
char* omni_stringify(Obj* obj);

void omni_gc_collect(Env* root_env, Obj* root_obj);
size_t omni_heap_count(void);

#endif
