#include "../src/engine.h"

static int assert_count = 0;

static void check(bool condition, const char* msg) {
    assert_count++;
    if (!condition) {
        fprintf(stderr, "ASSERTION FAILED: %s\n", msg);
        exit(1);
    } else {
        printf("  [PASS] %s\n", msg);
    }
}

int main(void) {
    printf("Running OmniLisp Engine Unit Tests...\n");
    omni_init();

    Env* env = omni_env_create(NULL);

    // 1. Integer allocation and evaluation
    Obj* i = omni_make_int(42);
    check(i->type == OBJ_INT, "integer object type check");
    check(i->as.ival == 42, "integer object value check");

    // 2. Float allocation and evaluation
    Obj* f = omni_make_float(3.14);
    check(f->type == OBJ_FLOAT, "float object type check");
    check(f->as.fval == 3.14, "float object value check");

    // 3. Boolean allocation
    Obj* b = omni_make_bool(true);
    check(b->type == OBJ_BOOL, "boolean object type check");
    check(b->as.bval == true, "boolean object value check");

    // 4. Symbol allocation and lookup
    Obj* sym = omni_make_symbol("x");
    check(sym->type == OBJ_SYMBOL, "symbol object type check");
    check(strcmp(sym->as.sval, "x") == 0, "symbol name check");

    // 5. Environment binding and resolution
    omni_env_bind(env, "x", omni_make_int(100));
    Obj* lookup = omni_env_lookup(env, "x");
    check(lookup != NULL, "environment lookup non-null");
    check(lookup->type == OBJ_INT, "environment lookup type check");
    check(lookup->as.ival == 100, "environment lookup value check");

    // 6. String allocation
    Obj* str = omni_make_string("hello");
    check(str->type == OBJ_STRING, "string object type check");
    check(strcmp(str->as.sval, "hello") == 0, "string value check");

    // 7. Cons cell creation
    Obj* cons = omni_make_cons(omni_make_int(1), omni_make_int(2));
    check(cons->type == OBJ_CONS, "cons object type check");
    check(cons->as.cons.car->as.ival == 1, "cons car check");
    check(cons->as.cons.cdr->as.ival == 2, "cons cdr check");

    // 8. Stringification
    char* s_str = omni_stringify(cons);
    check(s_str != NULL, "stringify non-null");
    check(strcmp(s_str, "(1 . 2)") == 0, "stringify cons format check");
    free(s_str);

    // 9. Garbage collection stress check
    size_t initial_heap = omni_heap_count();
    check(initial_heap > 0, "heap count active");
    omni_gc_collect(env, cons);
    size_t post_gc_heap = omni_heap_count();
    check(post_gc_heap <= initial_heap, "garbage collection reclaims or retains properly");

    omni_cleanup();
    printf("All %d engine unit tests passed successfully.\n", assert_count);
    return 0;
}
