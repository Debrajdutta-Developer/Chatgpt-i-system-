#include "../src/engine.h"

static int assert_count = 0;

static void check(bool condition, const char* msg) {
    assert_count++;
    if (!condition) {
        fprintf(stderr, "INTEGRATION ASSERTION FAILED: %s\n", msg);
        exit(1);
    } else {
        printf("  [PASS] %s\n", msg);
    }
}

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

int main(void) {
    printf("Running OmniLisp Integration Tests...\n");
    omni_init();
    Env* env = omni_env_create(NULL);
    omni_env_bind(env, "+", omni_make_primitive(prim_add));

    // 1. Read integer literal
    Obj* expr1 = omni_read("42");
    check(expr1->type == OBJ_INT, "read integer literal");
    Obj* res1 = omni_eval(expr1, env);
    check(res1->as.ival == 42, "eval integer literal");

    // 2. Read boolean literal
    Obj* expr2 = omni_read("true");
    check(expr2->type == OBJ_BOOL, "read boolean literal");
    Obj* res2 = omni_eval(expr2, env);
    check(res2->as.bval == true, "eval boolean literal");

    // 3. Read quoted expression
    Obj* expr3 = omni_read("'(a b c)");
    check(expr3->type == OBJ_CONS, "read quote expression");
    Obj* res3 = omni_eval(expr3, env);
    check(res3->type == OBJ_CONS, "eval quote list");

    // 4. Evaluate conditional (if true)
    Obj* expr4 = omni_read("(if true 10 20)");
    Obj* res4 = omni_eval(expr4, env);
    check(res4->as.ival == 10, "eval if-true branch");

    // 5. Evaluate conditional (if false)
    Obj* expr5 = omni_read("(if false 10 20)");
    Obj* res5 = omni_eval(expr5, env);
    check(res5->as.ival == 20, "eval if-false branch");

    // 6. Define variable binding
    Obj* expr6 = omni_read("(define a 150)");
    omni_eval(expr6, env);
    Obj* lookup_a = omni_env_lookup(env, "a");
    check(lookup_a != NULL, "define binds variable in env");
    check(lookup_a->as.ival == 150, "define value check");

    // 7. Primitive arithmetic evaluation
    Obj* expr7 = omni_read("(+ 10 20 30)");
    Obj* res7 = omni_eval(expr7, env);
    check(res7->as.ival == 60, "evaluate primitive addition");

    // 8. Lambda closure creation and application
    Obj* expr8 = omni_read("((lambda (x) (+ x 5)) 40)");
    Obj* res8 = omni_eval(expr8, env);
    check(res8->as.ival == 45, "lambda creation and application check");

    // 9. Begin block sequencing
    Obj* expr9 = omni_read("(begin (define b 99) b)");
    Obj* res9 = omni_eval(expr9, env);
    check(res9->as.ival == 99, "begin block sequence evaluation");

    omni_cleanup();
    printf("All %d integration tests passed successfully.\n", assert_count);
    return 0;
}
