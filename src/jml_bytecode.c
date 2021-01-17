#include <stdio.h>

#include <jml_gc.h>
#include <jml_bytecode.h>
#include <jml_vm.h>


void
jml_bytecode_init(jml_bytecode_t *bytecode)
{
    bytecode->count = 0;
    bytecode->code = NULL;
    bytecode->lines = NULL;
    bytecode->capacity = 0;
    jml_value_array_init(&bytecode->constants);
}


void
jml_bytecode_write(jml_bytecode_t *bytecode,
    uint8_t byte, uint16_t line)
{
    if (bytecode->capacity < bytecode->count + 1) {
        int old_capacity = bytecode->capacity;
        bytecode->capacity = GROW_CAPACITY(old_capacity);
        bytecode->code = GROW_ARRAY(uint8_t, bytecode->code,
            old_capacity, bytecode->capacity);
        bytecode->lines = GROW_ARRAY(uint16_t , bytecode->lines,
            old_capacity, bytecode->capacity);
    }

    bytecode->code[bytecode->count] = byte;
    bytecode->lines[bytecode->count] = line;
    bytecode->count++;
}


void
jml_bytecode_free(jml_bytecode_t *bytecode)
{
    FREE_ARRAY(uint8_t, bytecode->code, bytecode->capacity);
    FREE_ARRAY(int, bytecode->lines, bytecode->capacity);
    jml_value_array_free(&bytecode->constants);
    jml_bytecode_init(bytecode);
}


int
jml_bytecode_add_const(jml_bytecode_t *bytecode,
    jml_value_t value)
{
    jml_vm_push(value);
    jml_value_array_write(&bytecode->constants, value);
    jml_vm_pop();

    return bytecode->constants.count - 1;
}


void
jml_bytecode_disassemble(jml_bytecode_t *bytecode,
    const char *name)
{
    printf(
        "======   %s   ======\n"
        "OFFSET   LINE   OPCODE               DATA\n",
        name
    );

    for (int offset = 0; offset < bytecode->count;) {
        offset = jml_bytecode_instruction_disassemble(
            bytecode, offset
        );
    }
}


static int
jml_bytecode_instruction_simple(const char *name,
    int offset)
{
    printf("%s\n", name);

    return offset + 1;
}


static int
jml_bytecode_instruction_byte(const char *name,
    jml_bytecode_t *bytecode, int offset)
{
    uint8_t slot = bytecode->code[offset + 1];
    printf("%-16s   %4d\n", name, slot);

    return offset + 2;
}


static int
jml_bytecode_instruction_jump(const char *name,
    int sign, jml_bytecode_t *bytecode, int offset)
{
    uint16_t jump = (uint16_t)(bytecode->code[offset + 1] << 8);
    jump |= bytecode->code[offset + 2];

    printf("%-16s   %4d -> %d\n",
        name, offset, offset + 3 + sign * jump
    );

    return offset + 3;
}


static int
jml_bytecode_instruction_invoke(const char *name,
    jml_bytecode_t *bytecode, int offset)
{
    uint8_t constant    = bytecode->code[offset + 1];
    uint8_t arg_count   = bytecode->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, arg_count, constant);
    jml_value_print(bytecode->constants.values[constant]);
    printf("'\n");

    return offset + 3;
}


static int
jml_bytecode_instruction_swap(const char *name,
    jml_bytecode_t *bytecode, int offset)
{
    uint8_t old_name    = bytecode->code[offset + 1];
    uint8_t new_name    = bytecode->code[offset + 2];
    printf("%-16s %4d '", name, old_name);

    jml_value_print(bytecode->constants.values[old_name]);
    printf("'   -> %4d '", new_name);
    jml_value_print(bytecode->constants.values[new_name]);
    printf("'\n");

    return offset + 3;
}


static int
jml_bytecode_instruction_const(const char *name,
    jml_bytecode_t *bytecode, int offset)
{
    uint8_t constant = bytecode->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    jml_value_print(bytecode->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}


int
jml_bytecode_instruction_disassemble(
    jml_bytecode_t *bytecode, int offset)
{
    printf("%04d    ", offset);
    if (offset > 0 &&
        bytecode->lines[offset] == bytecode->lines[offset - 1]) {

        printf("   |    ");
    } else {
        printf("%4d    ", bytecode->lines[offset]);
    }

    uint8_t instruction = bytecode->code[offset];
    switch (instruction) {
        case OP_POP:
            return jml_bytecode_instruction_simple("OP_POP", offset);

        case OP_POP_TWO:
            return jml_bytecode_instruction_simple("OP_POP_TWO", offset);

        case OP_ROT:
            return jml_bytecode_instruction_simple("OP_ROT", offset);

        case OP_CONST:
            return jml_bytecode_instruction_const("OP_CONST", bytecode, offset);

        case OP_NONE:
            return jml_bytecode_instruction_simple("OP_NONE", offset);

        case OP_TRUE:
            return jml_bytecode_instruction_simple("OP_TRUE", offset);

        case OP_FALSE:
            return jml_bytecode_instruction_simple("OP_FALSE", offset);

        case OP_ADD:
            return jml_bytecode_instruction_simple("OP_ADD", offset);

        case OP_SUB:
            return jml_bytecode_instruction_simple("OP_SUB", offset);

        case OP_MUL:
            return jml_bytecode_instruction_simple("OP_MUL", offset);

        case OP_DIV:
            return jml_bytecode_instruction_simple("OP_DIV", offset);

        case OP_POW:
            return jml_bytecode_instruction_simple("OP_POW", offset);

        case OP_MOD:
            return jml_bytecode_instruction_simple("OP_MOD", offset);

        case OP_NOT:
            return jml_bytecode_instruction_simple("OP_NOT", offset);

        case OP_NEGATE:
            return jml_bytecode_instruction_simple("OP_NEGATE", offset);

        case OP_EQUAL:
            return jml_bytecode_instruction_simple("OP_EQUAL", offset);

        case OP_GREATER:
            return jml_bytecode_instruction_simple("OP_GREATER", offset);

        case OP_GREATEREQ:
            return jml_bytecode_instruction_simple("OP_GREATEREQ", offset);

        case OP_LESS:
            return jml_bytecode_instruction_simple("OP_LESS", offset);

        case OP_LESSEQ:
            return jml_bytecode_instruction_simple("OP_LESSEQ", offset);

        case OP_NOTEQ:
            return jml_bytecode_instruction_simple("OP_NOTEQ", offset);

        case OP_CONTAIN:
            return jml_bytecode_instruction_simple("OP_CONTAIN", offset);

        case OP_JUMP:
            return jml_bytecode_instruction_jump("OP_JUMP", 1, bytecode, offset);

        case OP_JUMP_IF_FALSE:
            return jml_bytecode_instruction_jump("OP_JUMP_IF_FALSE", 1, bytecode, offset);

        case OP_LOOP:
            return jml_bytecode_instruction_jump("OP_LOOP", -1, bytecode, offset);

        case OP_CALL:
            return jml_bytecode_instruction_byte("OP_CALL", bytecode, offset);

        case OP_METHOD:
            return jml_bytecode_instruction_const("OP_METHOD", bytecode, offset);

        case OP_INVOKE:
            return jml_bytecode_instruction_invoke("OP_INVOKE", bytecode, offset);

        case OP_SUPER_INVOKE:
            return jml_bytecode_instruction_invoke("OP_SUPER_INVOKE", bytecode, offset);

        case OP_CLOSURE: {
            offset++;
            uint8_t constant = bytecode->code[offset++];
            printf("%-16s   %4d   ", "OP_CLOSURE", constant);
            jml_value_print(bytecode->constants.values[constant]);
            printf("\n");

            jml_obj_function_t *function = AS_FUNCTION(bytecode->constants.values[constant]);

            for (int j = 0; j < function->upvalue_count; j++) {
                int local = bytecode->code[offset++];
                int index = bytecode->code[offset++];
                printf("%04d    |                   %s %d\n",
                    offset - 2, local ? "local" : "upvalue", index);
            }
            return offset;
        }

        case OP_RETURN:
            return jml_bytecode_instruction_simple("OP_RETURN", offset);

        case OP_CLASS:
            return jml_bytecode_instruction_const("OP_CLASS", bytecode, offset);

        case OP_INHERIT:
            return jml_bytecode_instruction_simple("OP_INHERIT", offset);

        case OP_SET_LOCAL:
            return jml_bytecode_instruction_byte("OP_SET_LOCAL", bytecode, offset);

        case OP_GET_LOCAL:
            return jml_bytecode_instruction_byte("OP_GET_LOCAL", bytecode, offset);

        case OP_SET_UPVALUE:
            return jml_bytecode_instruction_byte("OP_SET_UPVALUE", bytecode, offset);

        case OP_GET_UPVALUE:
            return jml_bytecode_instruction_byte("OP_GET_UPVALUE", bytecode, offset);

        case OP_CLOSE_UPVALUE:
            return jml_bytecode_instruction_simple("OP_CLOSE_UPVALUE", offset);

        case OP_SET_GLOBAL:
            return jml_bytecode_instruction_const("OP_SET_GLOBAL", bytecode, offset);

        case OP_GET_GLOBAL:
            return jml_bytecode_instruction_const("OP_GET_GLOBAL", bytecode, offset);

        case OP_DEF_GLOBAL:
            return jml_bytecode_instruction_const("OP_DEF_GLOBAL", bytecode, offset);

        case OP_SET_MEMBER:
            return jml_bytecode_instruction_const("OP_SET_MEMBER", bytecode, offset);

        case OP_GET_MEMBER:
            return jml_bytecode_instruction_const("OP_GET_MEMBER", bytecode, offset);

        case OP_SET_INDEX:
            return jml_bytecode_instruction_const("OP_SET_INDEX", bytecode, offset);

        case OP_GET_INDEX:
            return jml_bytecode_instruction_const("OP_GET_INDEX", bytecode, offset);

        case OP_SWAP_GLOBAL:
            return jml_bytecode_instruction_swap("OP_SWAP_GLOBAL", bytecode, offset);

        case OP_SWAP_LOCAL:
            return jml_bytecode_instruction_swap("OP_SWAP_LOCAL", bytecode, offset);

        case OP_SUPER:
            return jml_bytecode_instruction_const("OP_SUPER", bytecode, offset);

        case OP_ARRAY:
            return jml_bytecode_instruction_byte("OP_ARRAY", bytecode, offset);

        case OP_MAP:
            return jml_bytecode_instruction_byte("OP_MAP", bytecode, offset);

        case OP_IMPORT_GLOBAL:
            return jml_bytecode_instruction_const("OP_IMPORT_GLOBAL", bytecode, offset);

        default:
            printf("unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
