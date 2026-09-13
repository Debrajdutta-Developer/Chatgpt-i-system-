#include "engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_arithmetic_execution(void) {
    VM vm;
    initVM(&vm);

    const char* source = "var a = 10; var b = 20; var c = a + b * 2; print c;";
    InterpretResult result = interpret(&vm, source);
    assert(result == INTERPRET_OK);

    Value val;
    ObjString* key = copyString(&vm, "c", 1);
    assert(tableGet(&vm.globals, key, &val));
    assert(is_number(val));
    assert(val.as.number == 50.0);

    freeVM(&vm);
}

static void test_string_concatenation(void) {
    VM vm;
    initVM(&vm);

    const char* source = "var hello = \"Hello, \"; var world = \"World!\"; var greeting = hello + world; print greeting;";
    InterpretResult result = interpret(&vm, source);
    assert(result == INTERPRET_OK);

    Value val;
    ObjString* key = copyString(&vm, "greeting", 8);
    assert(tableGet(&vm.globals, key, &val));
    assert(is_obj_type(val, OBJ_STRING));
    assert(strcmp(((ObjString*)val.as.obj)->chars, "Hello, World!") == 0);

    freeVM(&vm);
}

static void test_control_flow(void) {
    VM vm;
    initVM(&vm);

    const char* source = 
        "var x = 10;\n"
        "var y = 0;\n"
        "if (x > 5) {\n"
        "  y = 100;\n"
        "} else {\n"
        "  y = 200;\n"
        "}\n";
    InterpretResult result = interpret(&vm, source);
    assert(result == INTERPRET_OK);

    Value val;
    ObjString* key = copyString(&vm, "y", 1);
    assert(tableGet(&vm.globals, key, &val));
    assert(is_number(val));
    assert(val.as.number == 100.0);

    freeVM(&vm);
}

static void test_while_loop(void) {
    VM vm;
    initVM(&vm);

    const char* source = 
        "var i = 0;\n"
        "var sum = 0;\n"
        "while (i < 5) {\n"
        "  sum = sum + i;\n"
        "  i = i + 1;\n"
        "}\n";
    InterpretResult result = interpret(&vm, source);
    assert(result == INTERPRET_OK);

    Value val_sum;
    ObjString* key_sum = copyString(&vm, "sum", 3);
    assert(tableGet(&vm.globals, key_sum, &val_sum));
    assert(is_number(val_sum));
    assert(val_sum.as.number == 10.0);

    freeVM(&vm);
}

static void test_function_calls(void) {
    VM vm;
    initVM(&vm);

    const char* source = 
        "fun add(a, b) {\n"
        "  return a + b;\n"
        "}\n"
        "var result = add(15, 25);\n";
    InterpretResult result = interpret(&vm, source);
    assert(result == INTERPRET_OK);

    Value val;
    ObjString* key = copyString(&vm, "result", 6);
    assert(tableGet(&vm.globals, key, &val));
    assert(is_number(val));
    assert(val.as.number == 40.0);

    freeVM(&vm);
}

int main(void) {
    printf("Running integration tests...\n");

    // 10+ explicit assertions across the integration suite
    test_arithmetic_execution();
    test_string_concatenation();
    test_control_flow();
    test_while_loop();
    test_function_calls();

    printf("All integration tests passed successfully!\n");
    return 0;
}
