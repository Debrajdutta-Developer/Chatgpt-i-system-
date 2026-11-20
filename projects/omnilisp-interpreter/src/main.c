#include "engine.h"

static Obj* prim_add(Env* env, Obj* args) {
    (void)env;
    int64_t sum = 0;
    while (args && args->type == OBJ_CONS) {
        Obj* item = args->as.cons.car;
        if (item->type == OBJ_INT) sum += item->as.ival;
        args = args->as.cons.cdr;
    }
    return omni_make_int(sum);
}

static Obj* prim_sub(Env* env, Obj* args) {
    (void)env;
    if (!args || args->type != OBJ_CONS) return omni_make_int(0);
    int64_t acc = args->as.cons.car->as.ival;
    args = args->as.cons.cdr;
    while (args && args->type == OBJ_CONS) {
        Obj* item = args->as.cons.car;
        if (item->type == OBJ_INT) acc -= item->as.ival;
        args = args->as.cons.cdr;
    }
    return omni_make_int(acc);
}

static Obj* prim_mul(Env* env, Obj* args) {
    (void)env;
    int64_t prod = 1;
    while (args && args->type == OBJ_CONS) {
        Obj* item = args->as.cons.car;
        if (item->type == OBJ_INT) prod *= item->as.ival;
        args = args->as.cons.cdr;
    }
    return omni_make_int(prod);
}

static Obj* prim_eq(Env* env, Obj* args) {
    (void)env;
    if (!args || args->type != OBJ_CONS || !args->as.cons.cdr || args->as.cons.cdr->type != OBJ_CONS) {
        return omni_make_bool(false);
    }
    Obj* a = args->as.cons.car;
    Obj* b = args->as.cons.cdr->as.cons.car;
    if (a->type == OBJ_INT && b->type == OBJ_INT) {
        return omni_make_bool(a->as.ival == b->as.ival);
    }
    return omni_make_bool(a == b);
}

static Obj* prim_car(Env* env, Obj* args) {
    (void)env;
    if (!args || args->type != OBJ_CONS) return omni_nil();
    Obj* lst = args->as.cons.car;
    if (lst && lst->type == OBJ_CONS) return lst->as.cons.car;
    return omni_nil();
}

static Obj* prim_cdr(Env* env, Obj* args) {
    (void)env;
    if (!args || args->type != OBJ_CONS) return omni_nil();
    Obj* lst = args->as.cons.car;
    if (lst && lst->type == OBJ_CONS) return lst->as.cons.cdr;
    return omni_nil();
}

static Obj* prim_cons(Env* env, Obj* args) {
    (void)env;
    if (!args || args->type != OBJ_CONS || !args->as.cons.cdr || args->as.cons.cdr->type != OBJ_CONS) {
        return omni_nil();
    }
    return omni_make_cons(args->as.cons.car, args->as.cons.cdr->as.cons.car);
}

static void register_primitives(Env* env) {
    omni_env_bind(env, "+", omni_make_primitive(prim_add));
    omni_env_bind(env, "-", omni_make_primitive(prim_sub));
    omni_env_bind(env, "*", omni_make_primitive(prim_mul));
    omni_env_bind(env, "=", omni_make_primitive(prim_eq));
    omni_env_bind(env, "car", omni_make_primitive(prim_car));
    omni_env_bind(env, "cdr", omni_make_primitive(prim_cdr));
    omni_env_bind(env, "cons", omni_make_primitive(prim_cons));
}

int main(int argc, char** argv) {
    omni_init();
    Env* global_env = omni_env_create(NULL);
    register_primitives(global_env);

    if (argc > 1) {
        FILE* f = fopen(argv[1], "r");
        if (!f) {
            fprintf(stderr, "Could not open file: %s\n", argv[1]);
            omni_cleanup();
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = malloc(len + 1);
        fread(buf, 1, len, f);
        buf[len] = '\0';
        fclose(f);

        Obj* expr = omni_read(buf);
        while (expr && expr->type != OBJ_EOF) {
            Obj* res = omni_eval(expr, global_env);
            char* s = omni_stringify(res);
            printf("%s\n", s);
            free(s);
            expr = omni_read(buf + (size_t)(expr - (Obj*)0)); // simple stream advance simulation
            break;
        }
        free(buf);
        omni_cleanup();
        return 0;
    }

    printf("OmniLisp Interactive REPL (C11)\nType expressions to evaluate.\n");
    char line[1024];
    while (true) {
        printf("omni> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        if (strncmp(line, "(exit)", 6) == 0) break;
        Obj* expr = omni_read(line);
        if (expr && expr->type != OBJ_EOF) {
            Obj* res = omni_eval(expr, global_env);
            char* s = omni_stringify(res);
            printf("%s\n", s);
            free(s);
        }
        omni_gc_collect(global_env, NULL);
    }

    omni_cleanup();
    return 0;
}
