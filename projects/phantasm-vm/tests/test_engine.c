#include "../src/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
    printf("Running Phantasm-VM Engine Unit Tests...\n");

    VMachine vm;
    vm_init(&vm);

    // Assertion 1: Initial PC and SP correctness
    assert(vm.state.pc == 0);
    assert(vm.state.sp == VM_MEMORY_SIZE - 4);

    // Assertion 2: Load program validation
    uint32_t prog[] = {
        (OP_LOADI << 24) | (0 << 20) | 5,
        (OP_LOADI << 24) | (1 << 20) | 10,
        (OP_ADD   << 24) | (0 << 20) | (1 << 16),
        (OP_HALT  << 24)
    };
    VMError err = vm_load_program(&vm, prog, 4, 0x0);
    assert(err == VM_OK);

    // Assertion 3: Step forward LOADI R0, 5
    err = vm_step_forward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[0] == 5);

    // Assertion 4: Step forward LOADI R1, 10
    err = vm_step_forward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[1] == 10);

    // Assertion 5: Step forward ADD R0, R1
    err = vm_step_forward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[0] == 15);

    // Assertion 6: Step backward to previous state (ADD undo)
    err = vm_step_backward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[0] == 5);

    // Assertion 7: Step backward again (LOADI R1 undo)
    err = vm_step_backward(&vm);
    assert(err == VM_OK);
    assert(vm.state.registers[1] == 0);

    // Assertion 8: Breakpoint addition and triggering
    vm_init(&vm);
    vm_load_program(&vm, prog, 4, 0x0);
    vm_add_breakpoint(&vm, 0, 5);
    vm_step_forward(&vm); // Sets R0 = 5
    err = vm_step_forward(&vm); // Should hit breakpoint on R0 == 5 before execution
    assert(err == VM_ERR_BREAKPOINT_HIT);

    // Assertion 9: Disassembler verification
    char dis_buf[64];
    vm_disassemble_instruction((OP_LOADI << 24) | (3 << 20) | 42, dis_buf, sizeof(dis_buf));
    assert(dis_buf[0] != '\0');

    // Assertion 10: Error string conversion validity
    assert(vm_strerror(VM_ERR_INVALID_OPCODE) != NULL);
    assert(vm_strerror(VM_OK) != NULL);

    printf("All 10 unit test assertions passed successfully.\n");
    return 0;
}
