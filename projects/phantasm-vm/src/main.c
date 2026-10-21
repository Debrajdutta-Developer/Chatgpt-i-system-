#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("Starting Phantasm-VM Time-Travel Engine CLI...\n");

    VMachine vm;
    vm_init(&vm);

    // Program: 
    // 0: LOADI R0, 10
    // 1: LOADI R1, 20
    // 2: ADD R0, R1
    // 3: PUSH R0
    // 4: POP R2
    // 5: HALT
    uint32_t program[] = {
        (OP_LOADI << 24) | (0 << 20) | (0 << 16) | 10,
        (OP_LOADI << 24) | (1 << 20) | (0 << 16) | 20,
        (OP_ADD   << 24) | (0 << 20) | (1 << 16) | 0,
        (OP_PUSH  << 24) | (0 << 20) | (0 << 16) | 0,
        (OP_POP   << 24) | (2 << 20) | (0 << 16) | 0,
        (OP_HALT  << 24) | (0 << 20) | (0 << 16) | 0
    };

    size_t prog_len = sizeof(program) / sizeof(program[0]);
    VMError err = vm_load_program(&vm, program, prog_len, 0x100);
    if (err != VM_OK) {
        fprintf(stderr, "Failed to load program: %s\n", vm_strerror(err));
        return 1;
    }

    printf("Program loaded successfully at 0x100. Disassembly:\n");
    for (size_t i = 0; i < prog_len; ++i) {
        char buf[64];
        vm_disassemble_instruction(program[i], buf, sizeof(buf));
        printf("  0x%03X: %s\n", (unsigned int)(0x100 + i * 4), buf);
    }

    printf("\n--- Executing Forward ---\n");
    for (int step = 1; step <= 5; ++step) {
        err = vm_step_forward(&vm);
        if (err != VM_OK) {
            fprintf(stderr, "Execution error at step %d: %s\n", step, vm_strerror(err));
            break;
        }
        printf("Step %d: PC=0x%03X, R0=%u, R1=%u, R2=%u, SP=0x%03X\n",
               step,
               (unsigned int)vm.state.pc,
               (unsigned int)vm.state.registers[0],
               (unsigned int)vm.state.registers[1],
               (unsigned int)vm.state.registers[2],
               (unsigned int)vm.state.sp);
    }

    printf("\n--- Executing Time-Travel Step-Back (3 steps) ---\n");
    for (int back = 1; back <= 3; ++back) {
        err = vm_step_backward(&vm);
        if (err != VM_OK) {
            fprintf(stderr, "Step-back error: %s\n", vm_strerror(err));
            break;
        }
        printf("Step-Back %d: PC=0x%03X, R0=%u, R1=%u, R2=%u, SP=0x%03X\n",
               back,
               (unsigned int)vm.state.pc,
               (unsigned int)vm.state.registers[0],
               (unsigned int)vm.state.registers[1],
               (unsigned int)vm.state.registers[2],
               (unsigned int)vm.state.sp);
    }

    printf("\nPhantasm-VM demonstration completed successfully.\n");
    return 0;
}
