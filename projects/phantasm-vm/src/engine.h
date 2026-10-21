#ifndef PHANTASM_VM_ENGINE_H
#define PHANTASM_VM_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VM_REG_COUNT 16
#define VM_MEMORY_SIZE (64 * 1024)
#define VM_MAX_SNAPSHOTS 256
#define VM_MAX_BREAKPOINTS 32

typedef enum {
    VM_OK = 0,
    VM_ERR_INVALID_OPCODE,
    VM_ERR_OUT_OF_BOUNDS,
    VM_ERR_UNALIGNED,
    VM_ERR_STACK_OVERFLOW,
    VM_ERR_SNAPSHOT_FULL,
    VM_ERR_NO_SNAPSHOT,
    VM_ERR_BREAKPOINT_HIT,
    VM_ERR_DIVISION_BY_ZERO,
    VM_ERR_NULL_POINTER
} VMError;

typedef enum {
    OP_HALT = 0,
    OP_LOADI,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_SHL,
    OP_SHR,
    OP_LOAD,
    OP_STORE,
    OP_JMP,
    OP_JZ,
    OP_JNZ,
    OP_PUSH,
    OP_POP,
    OP_CALL,
    OP_RET,
    OP_PRINT
} VMOpcode;

typedef struct {
    uint32_t registers[VM_REG_COUNT];
    uint32_t pc;
    uint32_t sp;
    uint32_t fp;
    uint32_t flags;
    uint64_t cycle;
} VMRegisterState;

typedef struct {
    uint32_t address;
    uint8_t old_value;
    uint8_t new_value;
} VMMemoryDelta;

#define VM_MAX_DELTAS_PER_STEP 8

typedef struct {
    VMRegisterState reg_state;
    size_t delta_count;
    VMMemoryDelta deltas[VM_MAX_DELTAS_PER_STEP];
} VMSnapshot;

typedef struct {
    uint32_t reg_index;
    uint32_t target_value;
    bool active;
} VMBreakpoint;

typedef struct {
    uint8_t memory[VM_MEMORY_SIZE];
    VMRegisterState state;
    VMSnapshot snapshots[VM_MAX_SNAPSHOTS];
    size_t snapshot_count;
    VMBreakpoint breakpoints[VM_MAX_BREAKPOINTS];
    bool halted;
    VMError last_error;
} VMachine;

void vm_init(VMachine *vm);
VMError vm_load_program(VMachine *vm, const uint32_t *bytecode, size_t count, uint32_t load_address);
VMError vm_step_forward(VMachine *vm);
VMError vm_step_backward(VMachine *vm);
VMError vm_run(VMachine *vm, uint32_t max_steps);

VMError vm_add_breakpoint(VMachine *vm, uint32_t reg_index, uint32_t target_value);
void vm_clear_breakpoints(VMachine *vm);

void vm_disassemble_instruction(uint32_t instruction, char *buf, size_t buf_size);

const char *vm_strerror(VMError err);

#endif
