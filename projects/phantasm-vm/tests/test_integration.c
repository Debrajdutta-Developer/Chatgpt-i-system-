#include "../src/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
    printf("Running Phantasm-VM Integration Tests...\n");

    VMachine vm;
    vm_init(&vm);

    // Integration Program: Arithmetic, Stack operations, and memory store/load
    uint32_t program[] = {
        (OP_LOADI << 24) | (0 << 20) | 100,  // R0 = 100
        (OP_LOADI << 24) | (1 << 20) | 4,    // R1 = 4 (memory offset)
        (OP_STORE << 24) | (0 << 20) | (1 << 16) | 0, // [R1 + 0] = R0 (Store 100 at addr 4)
        (OP_LOAD  << 24) | (2 << 20) | (1 << 16) | 0, // R2 = [R1 + 0] (Load R2 from addr 4)
        (OP_PUSH  << 24) | (2 << 20) | 0,    // Push R2 onto stack
        (OP_POP   << 24) | (3 << 20) | 0,    // Pop stack into R3
        (OP_HALT  << 24) | 0                 // Halt
    };

    size_t count = sizeof(program) / sizeof(program[0]);
    VMError err = vm_load_program(&vm, program, count, 0x200);
    assert(err == VM_OK);

    // Assertion 1: Step 1 (LOADI R0, 100)
    err = vm_step_forward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[0] == 100);

    // Assertion 2: Step 2 (LOADI R1, 4)
    err = vm_step_forward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[1] == 4);

    // Assertion 3: Step 3 (STORE [R1], R0)
    err = vm_step_forward(&vm);
    assert(err == VM_OK);

    // Assertion 4: Step 4 (LOAD R2, [R1])
    err = vm_step_forward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[2] == 100);

    // Assertion 5: Step 5 (PUSH R2)
    err = vm_step_forward(&vm);
    assert(err == VM_OK);

    // Assertion 6: Step 6 (POP R3)
    err = vm_step_forward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[3] == 100);

    // Assertion 7: Step 7 (HALT)
    err = vm_step_forward(&vm);
    assert(err == VM_OK);
    assert(vm.halted == true);

    // Assertion 8: Time-travel backward through all 7 steps to initial state
    for (int i = 0; i < 7; ++i) {
        err = vm_step_backward(&vm);
        assert(err == VM_OK);
    }
    assert(vm.state.registers[0] == 0);
    assert(vm.state.pc == 0x200);

    // Assertion 9: Unaligned memory access error detection
    VMachine vm_err;
    vm_init(&vm_err);
    uint32_t unaligned_prog[] = {
        (OP_LOADI << 24) | (0 << 20) | 3, // R0 = 3 (unaligned address)
        (OP_LOAD  << 24) | (1 << 20) | (0 << 16) | 0 // Load from unaligned address [R0]
    };
    vm_load_program(&vm_err, unaligned_prog, 2, 0x0);
    vm_step_forward(&vm_err); // LOADI
    err = vm_step_forward(&vm_err); // LOAD should fail with unaligned
    assert(err == VM_ERR_UNALIGNED);

    // Assertion 10: Division by zero error detection
    VMachine vm_div;
    vm_init(&vm_div);
    uint32_t div_prog[] = {
        (OP_LOADI << 24) | (0 << 20) | 10,
        (OP_LOADI << 24) | (1 << 20) | 0,
        (OP_DIV   << 24) | (0 << 20) | (1 << 16)
    };
    vm_load_program(&vm_div, div_prog, 3, 0x0);
    vm_step_forward(&vm_div);
    vm_step_forward(&vm_div);
    err = vm_step_forward(&vm_div);
    assert(err == VM_ERR_DIVISION_BY_ZERO);

    printf(
        "All 10 integration test assertions passed successfully.\n"
    );
    return 0;
}
