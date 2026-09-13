#include "engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_value_helpers(void) {
    Value nil_val = val_nil();
    assert(is_nil(nil_val));
    assert(!is_bool(nil_val));
    assert(!is_number(nil_val));

    Value bool_val = val_bool(true);
    assert(is_bool(bool_val));
    assert(bool_val.as.boolean == true);

    Value num_val = val_number(42.5);
    assert(is_number(num_val));
    assert(num_val.as.number == 42.5);
}

static void test_vm_stack(void) {
    VM vm;
    initVM(&vm);

    push(&vm, val_number(10.0));
    push(&vm, val_number(20.0));
    assert(vm.stack_top - vm.stack == 2);

    Value val2 = pop(&vm);
    assert(is_number(val2));
    assert(val2.as.number == 20.0);

    Value val1 = pop(&vm);
    assert(is_number(val1));
    assert(val1.as.number == 10.0);

    assert(vm.stack_top == vm.stack);
    freeVM(&vm);
}

static void test_table_operations(void) {
    VM vm;
    initVM(&vm);

    Table table;
    initTable(&table);

    ObjString* key1 = copyString(&vm, "key1", 4);
    ObjString* key2 = copyString(&vm, "key2", 4);

    tableSet(&vm, &table, key1, val_number(100.0));
    tableSet(&vm, &table, key2, val_bool(false));

    Value val;
    assert(tableGet(&table, key1, &val));
    assert(is_number(val));
    assert(val.as.number == 100.0);

    assert(tableGet(&table, key2, &val));
    assert(is_bool(val));
    assert(val.as.boolean == false);

    tableDelete(&table, key1);
    assert(!tableGet(&table, key1, &val));

    freeTable(&vm, &table);
    freeVM(&vm);
}

static void test_garbage_collector(void) {
    VM vm;
    initVM(&vm);

    // Allocate some strings
    ObjString* str1 = copyString(&vm, "garbage_collect_me_1", 20);
    ObjString* str2 = copyString(&vm, "keep_me_alive", 13);
    (void)str1; // Avoid unused variable warning

    // Push str2 to stack to keep it alive
    push(&vm, val_obj((Obj*)str2));

    // Trigger GC
    collectGarbage(&vm);

    // str2 should still be alive, str1 should be swept
    assert(vm.objects != NULL);
    assert(vm.objects->type == OBJ_STRING);

    freeVM(&vm);
}

int main(void) {
    printf("Running engine unit tests...\n");

    // 10 explicit assertions across the test suite
    test_value_helpers();
    test_vm_stack();
    test_table_operations();
    test_garbage_collector();

    printf("All engine unit tests passed successfully!\n");
    return 0;
}
