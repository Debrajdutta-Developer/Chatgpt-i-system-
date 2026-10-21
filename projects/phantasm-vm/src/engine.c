#include "engine.h"
#include <string.h>
#include <stdio.h>

void vm_init(VMachine *vm) {
    if (!vm) return;
    memset(vm, 0, sizeof(VMachine));
    vm->state.sp = VM_MEMORY_SIZE - 4;
    vm->state.fp = VM_MEMORY_SIZE - 4;
    vm->halted = false;
    vm->last_error = VM_OK;
}

const char *vm_strerror(VMError err) {
    switch (err) {
        case VM_OK: return "Success";
        case VM_ERR_INVALID_OPCODE: return "Invalid opcode";
        case VM_ERR_OUT_OF_BOUNDS: return "Memory out of bounds";
        case VM_ERR_UNALIGNED: return "Unaligned memory access";
        case VM_ERR_STACK_OVERFLOW: return "Stack overflow/underflow";
        case VM_ERR_SNAPSHOT_FULL: return "Snapshot buffer full";
        case VM_ERR_NO_SNAPSHOT: return "No snapshot available for step-back";
        case VM_ERR_BREAKPOINT_HIT: return "Breakpoint hit";
        case VM_ERR_DIVISION_BY_ZERO: return "Division by zero";
        case VM_ERR_NULL_POINTER: return "Null pointer reference";
        default: return "Unknown error";
    }
}

VMError vm_load_program(VMachine *vm, const uint32_t *bytecode, size_t count, uint32_t load_address) {
    if (!vm || !bytecode) return VM_ERR_NULL_POINTER;
    if (load_address + count * 4 > VM_MEMORY_SIZE) return VM_ERR_OUT_OF_BOUNDS;
    
    for (size_t i = 0; i < count; ++i) {
        uint32_t instr = bytecode[i];
        uint32_t addr = load_address + (uint32_t)(i * 4);
        if (addr + 3 >= VM_MEMORY_SIZE) return VM_ERR_OUT_OF_BOUNDS;
        vm->memory[addr]     = (uint8_t)(instr & 0xFF);
        vm->memory[addr + 1] = (uint8_t)((instr >> 8) & 0xFF);
        vm->memory[addr + 2] = (uint8_t)((instr >> 16) & 0xFF);
        vm->memory[addr + 3] = (uint8_t)((instr >> 24) & 0xFF);
    }
    vm->state.pc = load_address;
    return VM_OK;
}

VMError vm_add_breakpoint(VMachine *vm, uint32_t reg_index, uint32_t target_value) {
    if (!vm) return VM_ERR_NULL_POINTER;
    if (reg_index >= VM_REG_COUNT) return VM_ERR_OUT_OF_BOUNDS;
    
    for (size_t i = 0; i < VM_MAX_BREAKPOINTS; ++i) {
        if (!vm->breakpoints[i].active) {
            vm->breakpoints[i].reg_index = reg_index;
            vm->breakpoints[i].target_value = target_value;
            vm->breakpoints[i].active = true;
            return VM_OK;
        }
    }
    return VM_ERR_SNAPSHOT_FULL;
}

void vm_clear_breakpoints(VMachine *vm) {
    if (!vm) return;
    for (size_t i = 0; i < VM_MAX_BREAKPOINTS; ++i) {
        vm->breakpoints[i].active = false;
    }
}

static bool check_breakpoints(const VMachine *vm) {
    for (size_t i = 0; i < VM_MAX_BREAKPOINTS; ++i) {
        if (vm->breakpoints[i].active) {
            uint32_t r = vm->breakpoints[i].reg_index;
            if (r < VM_REG_COUNT && vm->state.registers[r] == vm->breakpoints[i].target_value) {
                return true;
            }
        }
    }
    return false;
}

void vm_disassemble_instruction(uint32_t instruction, char *buf, size_t buf_size) {
    uint8_t op = (instruction >> 24) & 0xFF;
    uint8_t rd = (instruction >> 20) & 0x0F;
    uint8_t rs = (instruction >> 16) & 0x0F;
    uint16_t imm = instruction & 0xFFFF;
    int16_t simm = (int16_t)imm;

    switch (op) {
        case OP_HALT: snprintf(buf, buf_size, "halt"); break;
        case OP_LOADI: snprintf(buf, buf_size, "loadi r%d, %d", rd, simm); break;
        case OP_ADD: snprintf(buf, buf_size, "add r%d, r%d", rd, rs); break;
        case OP_SUB: snprintf(buf, buf_size, "sub r%d, r%d", rd, rs); break;
        case OP_MUL: snprintf(buf, buf_size, "mul r%d, r%d", rd, rs); break;
        case OP_DIV: snprintf(buf, buf_size, "div r%d, r%d", rd, rs); break;
        case OP_AND: snprintf(buf, buf_size, "and r%d, r%d", rd, rs); break;
        case OP_OR: snprintf(buf, buf_size, "or r%d, r%d", rd, rs); break;
        case OP_XOR: snprintf(buf, buf_size, "xor r%d, r%d", rd, rs); break;
        case OP_SHL: snprintf(buf, buf_size, "shl r%d, r%d", rd, rs); break;
        case OP_SHR: snprintf(buf, buf_size, "shr r%d, r%d", rd, rs); break;
        case OP_LOAD: snprintf(buf, buf_size, "load r%d, [r%d + %d]", rd, rs, simm); break;
        case OP_STORE: snprintf(buf, buf_size, "store [r%d + %d], r%d", rs, simm, rd); break;
        case OP_JMP: snprintf(buf, buf_size, "jmp %d", simm); break;
        case OP_JZ: snprintf(buf, buf_size, "jz r%d, %d", rd, simm); break;
        case OP_JNZ: snprintf(buf, buf_size, "jnz r%d, %d", rd, simm); break;
        case OP_PUSH: snprintf(buf, buf_size, "push r%d", rd); break;
        case OP_POP: snprintf(buf, buf_size, "pop r%d", rd); break;
        case OP_CALL: snprintf(buf, buf_size, "call %d", simm); break;
        case OP_RET: snprintf(buf, buf_size, "ret"); break;
        case OP_PRINT: snprintf(buf, buf_size, "print r%d", rd); break;
        default: snprintf(buf, buf_size, "unknown"); break;
    }
}

VMError vm_step_forward(VMachine *vm) {
    if (!vm) return VM_ERR_NULL_POINTER;
    if (vm->halted) return VM_ERR_NO_SNAPSHOT;

    if (check_breakpoints(vm)) {
        vm->last_error = VM_ERR_BREAKPOINT_HIT;
        return VM_ERR_BREAKPOINT_HIT;
    }

    if (vm->state.pc + 4 > VM_MEMORY_SIZE) {
        vm->last_error = VM_ERR_OUT_OF_BOUNDS;
        return VM_ERR_OUT_OF_BOUNDS;
    }

    uint32_t pc = vm->state.pc;
    uint32_t instruction = (uint32_t)vm->memory[pc] |
                           ((uint32_t)vm->memory[pc + 1] << 8) |
                           ((uint32_t)vm->memory[pc + 2] << 16) |
                           ((uint32_t)vm->memory[pc + 3] << 24);

    if (vm->snapshot_count >= VM_MAX_SNAPSHOTS) {
        vm->last_error = VM_ERR_SNAPSHOT_FULL;
        return VM_ERR_SNAPSHOT_FULL;
    }

    VMSnapshot *snap = &vm->snapshots[vm->snapshot_count];
    snap->reg_state = vm->state;
    snap->delta_count = 0;

    uint8_t op = (instruction >> 24) & 0xFF;
    uint8_t rd = (instruction >> 20) & 0x0F;
    uint8_t rs = (instruction >> 16) & 0x0F;
    uint16_t imm = instruction & 0xFFFF;
    int16_t simm = (int16_t)imm;

    vm->state.pc += 4;
    vm->state.cycle++;

    VMError err = VM_OK;

    switch (op) {
        case OP_HALT:
            vm->halted = true;
            break;

        case OP_LOADI:
            if (rd >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] = (uint32_t)simm;
            break;

        case OP_ADD:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] += vm->state.registers[rs];
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_SUB:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] -= vm->state.registers[rs];
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_MUL:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] *= vm->state.registers[rs];
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_DIV:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            if (vm->state.registers[rs] == 0) { err = VM_ERR_DIVISION_BY_ZERO; break; }
            vm->state.registers[rd] /= vm->state.registers[rs];
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_AND:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] &= vm->state.registers[rs];
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_OR:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] |= vm->state.registers[rs];
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_XOR:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] ^= vm->state.registers[rs];
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_SHL:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] <<= (vm->state.registers[rs] & 0x1F);
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_SHR:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            vm->state.registers[rd] >>= (vm->state.registers[rs] & 0x1F);
            vm->state.flags = (vm->state.registers[rd] == 0) ? 1 : 0;
            break;

        case OP_LOAD:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            {
                uint32_t addr = vm->state.registers[rs] + (uint32_t)simm;
                if (addr + 3 >= VM_MEMORY_SIZE) { err = VM_ERR_OUT_OF_BOUNDS; break; }
                if (addr % 4 != 0) { err = VM_ERR_UNALIGNED; break; }
                uint32_t val = (uint32_t)vm->memory[addr] |
                               ((uint32_t)vm->memory[addr + 1] << 8) |
                               ((uint32_t)vm->memory[addr + 2] << 16) |
                               ((uint32_t)vm->memory[addr + 3] << 24);
                vm->state.registers[rd] = val;
            }
            break;

        case OP_STORE:
            if (rd >= VM_REG_COUNT || rs >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            {
                uint32_t addr = vm->state.registers[rs] + (uint32_t)simm;
                if (addr + 3 >= VM_MEMORY_SIZE) { err = VM_ERR_OUT_OF_BOUNDS; break; }
                if (addr % 4 != 0) { err = VM_ERR_UNALIGNED; break; }
                uint32_t val = vm->state.registers[rd];
                for (int b = 0; b < 4; ++b) {
                    uint32_t target_addr = addr + (uint32_t)b;
                    if (snap->delta_count < VM_MAX_DELTAS_PER_STEP) {
                        snap->deltas[snap->delta_count].address = target_addr;
                        snap->deltas[snap->delta_count].old_value = vm->memory[target_addr];
                        snap->deltas[snap->delta_count].new_value = (uint8_t)((val >> (b * 8)) & 0xFF);
                        snap->delta_count++;
                    }
                    vm->memory[target_addr] = (uint8_t)((val >> (b * 8)) & 0xFF);
                }
            }
            break;

        case OP_JMP:
            vm->state.pc = (uint32_t)simm;
            if (vm->state.pc >= VM_MEMORY_SIZE) { err = VM_ERR_OUT_OF_BOUNDS; }
            break;

        case OP_JZ:
            if (rd >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            if (vm->state.registers[rd] == 0) {
                vm->state.pc = (uint32_t)simm;
                if (vm->state.pc >= VM_MEMORY_SIZE) { err = VM_ERR_OUT_OF_BOUNDS; }
            }
            break;

        case OP_JNZ:
            if (rd >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            if (vm->state.registers[rd] != 0) {
                vm->state.pc = (uint32_t)simm;
                if (vm->state.pc >= VM_MEMORY_SIZE) { err = VM_ERR_OUT_OF_BOUNDS; }
            }
            break;

        case OP_PUSH:
            if (rd >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            if (vm->state.sp < 4 || vm->state.sp >= VM_MEMORY_SIZE) { err = VM_ERR_STACK_OVERFLOW; break; }
            vm->state.sp -= 4;
            {
                uint32_t addr = vm->state.sp;
                uint32_t val = vm->state.registers[rd];
                for (int b = 0; b < 4; ++b) {
                    uint32_t target_addr = addr + (uint32_t)b;
                    if (snap->delta_count < VM_MAX_DELTAS_PER_STEP) {
                        snap->deltas[snap->delta_count].address = target_addr;
                        snap->deltas[snap->delta_count].old_value = vm->memory[target_addr];
                        snap->deltas[snap->delta_count].new_value = (uint8_t)((val >> (b * 8)) & 0xFF);
                        snap->delta_count++;
                    }
                    vm->memory[target_addr] = (uint8_t)((val >> (b * 8)) & 0xFF);
                }
            }
            break;

        case OP_POP:
            if (rd >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            if (vm->state.sp + 4 > VM_MEMORY_SIZE) { err = VM_ERR_STACK_OVERFLOW; break; }
            {
                uint32_t addr = vm->state.sp;
                uint32_t val = (uint32_t)vm->memory[addr] |
                               ((uint32_t)vm->memory[addr + 1] << 8) |
                               ((uint32_t)vm->memory[addr + 2] << 16) |
                               ((uint32_t)vm->memory[addr + 3] << 24);
                vm->state.registers[rd] = val;
                vm->state.sp += 4;
            }
            break;

        case OP_CALL:
            if (vm->state.sp < 4 || vm->state.sp >= VM_MEMORY_SIZE) { err = VM_ERR_STACK_OVERFLOW; break; }
            vm->state.sp -= 4;
            {
                uint32_t addr = vm->state.sp;
                uint32_t val = vm->state.pc;
                for (int b = 0; b < 4; ++b) {
                    uint32_t target_addr = addr + (uint32_t)b;
                    if (snap->delta_count < VM_MAX_DELTAS_PER_STEP) {
                        snap->deltas[snap->delta_count].address = target_addr;
                        snap->deltas[snap->delta_count].old_value = vm->memory[target_addr];
                        snap->deltas[snap->delta_count].new_value = (uint8_t)((val >> (b * 8)) & 0xFF);
                        snap->delta_count++;
                    }
                    vm->memory[target_addr] = (uint8_t)((val >> (b * 8)) & 0xFF);
                }
            }
            vm->state.pc = (uint32_t)simm;
            if (vm->state.pc >= VM_MEMORY_SIZE) { err = VM_ERR_OUT_OF_BOUNDS; }
            break;

        case OP_RET:
            if (vm->state.sp + 4 > VM_MEMORY_SIZE) { err = VM_ERR_STACK_OVERFLOW; break; }
            {
                uint32_t addr = vm->state.sp;
                uint32_t val = (uint32_t)vm->memory[addr] |
                               ((uint32_t)vm->memory[addr + 1] << 8) |
                               ((uint32_t)vm->memory[addr + 2] << 16) |
                               ((uint32_t)vm->memory[addr + 3] << 24);
                vm->state.pc = val;
                vm->state.sp += 4;
            }
            break;

        case OP_PRINT:
            if (rd >= VM_REG_COUNT) { err = VM_ERR_OUT_OF_BOUNDS; break; }
            printf("[VM PRINT] R%d = %u (0x%X)\n", rd, vm->state.registers[rd], vm->state.registers[rd]);
            break;

        default:
            err = VM_ERR_INVALID_OPCODE;
            break;
    }

    vm->last_error = err;
    if (err == VM_OK) {
        vm->snapshot_count++;
    }
    return err;
}

VMError vm_step_backward(VMachine *vm) {
    if (!vm) return VM_ERR_NULL_POINTER;
    if (vm->snapshot_count == 0) return VM_ERR_NO_SNAPSHOT;

    vm->snapshot_count--;
    VMSnapshot *snap = &vm->snapshots[vm->snapshot_count];

    for (size_t i = 0; i < snap->delta_count; ++i) {
        uint32_t addr = snap->deltas[i].address;
        if (addr < VM_MEMORY_SIZE) {
            vm->memory[addr] = snap->deltas[i].old_value;
        }
    }

    vm->state = snap->reg_state;
    vm->halted = false;
    vm->last_error = VM_OK;
    return VM_OK;
}

VMError vm_run(VMachine *vm, uint32_t max_steps) {
    if (!vm) return VM_ERR_NULL_POINTER;
    for (uint32_t i = 0; i < max_steps; ++i) {
        if (vm->halted) break;
        VMError err = vm_step_forward(vm);
        if (err != VM_OK) return err;
    }
    return VM_OK;
}
