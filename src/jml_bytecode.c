#include <stdio.h>

#include <jml_gc.h>
#include <jml_bytecode.h>
#include <jml_vm.h>


void
jml_bytecode_init(jml_bytecode_t *bytecode)
{
    bytecode->count         = 0;
    bytecode->code          = NULL;
    bytecode->lines         = NULL;
    bytecode->capacity      = 0;

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

    bytecode->code[bytecode->count]  = byte;
    bytecode->lines[bytecode->count] = line;
    ++bytecode->count;
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
    jml_gc_exempt_push(value);
    jml_value_array_write(&bytecode->constants, value);
    jml_gc_exempt_pop();

    return bytecode->constants.count - 1;
}


void
jml_bytecode_disassemble(jml_bytecode_t *bytecode,
    const char *name)
{
    uint16_t pad = (25 - strlen(name)) / 2;

    printf(
        "======   %*s%s%*s   ======\n"
        "OFFSET   LINE   OPCODE              DATA\n",
        pad, "", name, pad, ""
    );

    for (uint32_t offset = 0; offset < bytecode->count; ) {
        offset = jml_bytecode_instruction_disassemble(
            bytecode, offset
        );
    }
}


static uint32_t
jml_bytecode_instruction_simple(const char *name,
    uint32_t offset)
{
    printf("%s\n", name);

    return offset + 1;
}


static uint32_t
jml_bytecode_instruction_byte(const char *name,
    jml_bytecode_t *bytecode, uint32_t offset)
{
    printf("%-16s %4d\n", name, bytecode->code[offset + 1]);
    return offset + 2;
}


static uint32_t
jml_bytecode_instruction_short(const char *name,
    jml_bytecode_t *bytecode, uint32_t offset)
{
    uint16_t short1     = (uint16_t)(bytecode->code[offset + 1] << 8);
    short1              |= bytecode->code[offset + 2];

    printf("%-16s %d\n", name, short1);
    return offset + 3;
}


static uint32_t
jml_bytecode_instruction_jump(const char *name,
    int sign, jml_bytecode_t *bytecode, uint32_t offset)
{
    uint16_t jump       = (uint16_t)(bytecode->code[offset + 1] << 8);
    jump                |= bytecode->code[offset + 2];

    printf("%-16s %4d -> %d\n",
        name, offset, offset + 3 + sign * jump
    );

    return offset + 3;
}


static uint32_t
jml_bytecode_instruction_invoke(const char *name,
    jml_bytecode_t *bytecode, uint32_t offset)
{
    uint8_t constant    = bytecode->code[offset + 1];
    uint8_t arg_count   = bytecode->code[offset + 2];

    printf("%-16s (%d args) %4d '", name, arg_count, constant);
    jml_value_print(bytecode->constants.values[constant]);
    printf("'\n");

    return offset + 3;
}


static uint32_t
jml_bytecode_instruction_invoke_extended(const char *name,
    jml_bytecode_t *bytecode, uint32_t offset)
{
    uint16_t constant   = (uint16_t)(bytecode->code[offset + 1] << 8);
    constant            |= bytecode->code[offset + 2];

    uint16_t arg_count  = (uint16_t)(bytecode->code[offset + 3] << 8);
    arg_count           |= bytecode->code[offset + 4];

    printf("%-16s (%d args) %d '", name, arg_count, constant);
    jml_value_print(bytecode->constants.values[constant]);
    printf("'\n");

    return offset + 5;
}


static uint32_t
jml_bytecode_instruction_const(const char *name,
    jml_bytecode_t *bytecode, uint32_t offset)
{
    uint8_t constant    = bytecode->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    jml_value_print(bytecode->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}


static uint32_t
jml_bytecode_instruction_const_extended(const char *name,
    jml_bytecode_t *bytecode, uint32_t offset)
{
    uint16_t constant   = (uint16_t)(bytecode->code[offset + 1] << 8);
    constant            |= bytecode->code[offset + 2];

    printf("%-16s %d '", name, constant);
    jml_value_print(bytecode->constants.values[constant]);
    printf("'\n");

    return offset + 3;
}


static uint32_t
jml_bytecode_instruction_consts(const char *name,
    jml_bytecode_t *bytecode, uint32_t offset)
{
    uint8_t byte1       = bytecode->code[offset + 1];
    uint8_t byte2       = bytecode->code[offset + 2];
    printf("%-16s %4d '", name, byte1);

    jml_value_print(bytecode->constants.values[byte1]);
    printf("'    %4d '", byte2);
    jml_value_print(bytecode->constants.values[byte2]);
    printf("'\n");

    return offset + 3;
}


static uint32_t
jml_bytecode_instruction_consts_extended(const char *name,
    jml_bytecode_t *bytecode, uint32_t offset)
{
    uint16_t short1     = (uint16_t)(bytecode->code[offset + 1] << 8);
    short1              |= bytecode->code[offset + 2];

    uint16_t short2     = (uint16_t)(bytecode->code[offset + 3] << 8);
    short2              |= bytecode->code[offset + 4];

    printf("%-16s %4d '", name, short1);
    jml_value_print(bytecode->constants.values[short1]);
    printf("'    %4d '", short2);
    jml_value_print(bytecode->constants.values[short2]);
    printf("'\n");

    return offset + 5;
}


uint32_t
jml_bytecode_instruction_disassemble(jml_bytecode_t *bytecode, uint32_t offset)
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
        case OP_NOP:
            return jml_bytecode_instruction_simple("OP_NOP", offset);

        case OP_POP:
            return jml_bytecode_instruction_simple("OP_POP", offset);

        case OP_POP_TWO:
            return jml_bytecode_instruction_simple("OP_POP_TWO", offset);

        case OP_ROT:
            return jml_bytecode_instruction_simple("OP_ROT", offset);

        case OP_SAVE:
            return jml_bytecode_instruction_simple("OP_SAVE", offset);

        case OP_CONST:
            return jml_bytecode_instruction_const("OP_CONST", bytecode, offset);

        case EXTENDED_OP(OP_CONST):
            return jml_bytecode_instruction_const_extended("OP_CONST_EXTENDED", bytecode, offset);

        case OP_NONE:
            return jml_bytecode_instruction_simple("OP_NONE", offset);

        case OP_TRUE:
            return jml_bytecode_instruction_simple("OP_TRUE", offset);

        case OP_FALSE:
            return jml_bytecode_instruction_simple("OP_FALSE", offset);

        case OP_BOOL:
            return jml_bytecode_instruction_simple("OP_BOOL", offset);

        case OP_ADD:
            return jml_bytecode_instruction_simple("OP_ADD", offset);

        case OP_SUB:
            return jml_bytecode_instruction_simple("OP_SUB", offset);

        case OP_MUL:
            return jml_bytecode_instruction_simple("OP_MUL", offset);

        case OP_POW:
            return jml_bytecode_instruction_simple("OP_POW", offset);

        case OP_DIV:
            return jml_bytecode_instruction_simple("OP_DIV", offset);

        case OP_MOD:
            return jml_bytecode_instruction_simple("OP_MOD", offset);

        case OP_NOT:
            return jml_bytecode_instruction_simple("OP_NOT", offset);

        case OP_NEG:
            return jml_bytecode_instruction_simple("OP_NEG", offset);

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

        case OP_CONCAT:
            return jml_bytecode_instruction_simple("OP_CONCAT", offset);

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

        case OP_INVOKE:
            return jml_bytecode_instruction_invoke("OP_INVOKE", bytecode, offset);

        case EXTENDED_OP(OP_INVOKE):
            return jml_bytecode_instruction_invoke_extended("OP_INVOKE_EXTENDED", bytecode, offset);

        case OP_SUPER_INVOKE:
            return jml_bytecode_instruction_invoke("OP_SUPER_INVOKE", bytecode, offset);

        case EXTENDED_OP(OP_SUPER_INVOKE):
            return jml_bytecode_instruction_invoke_extended("OP_SUPER_INVOKE_EXTENDED", bytecode, offset);

        case OP_CLOSURE: {
            ++offset;
            uint8_t constant    = bytecode->code[offset++];

            printf("%-16s %4d   ", "OP_CLOSURE", constant);
            jml_value_print(bytecode->constants.values[constant]);
            printf("\n");

            jml_obj_function_t *function = AS_FUNCTION(bytecode->constants.values[constant]);

            for (int i = 0; i < function->upvalue_count; ++i) {
                int local       = bytecode->code[offset++];
                int index       = bytecode->code[offset++];

                printf("%04d       |    %-16s %4d\n",
                    offset - 2, local ? "local" : "upvalue", index);
            }
            return offset;
        }

        case EXTENDED_OP(OP_CLOSURE): {
            ++offset;
            uint16_t constant   = (uint16_t)(bytecode->code[offset++] << 8);
            constant            |= bytecode->code[offset++];

            printf("%-16s %4d   ", "OP_CLOSURE_EXTENDED", constant);
            jml_value_print(bytecode->constants.values[constant]);
            printf("\n");

            jml_obj_function_t *function = AS_FUNCTION(bytecode->constants.values[constant]);

            for (int i = 0; i < function->upvalue_count; ++i) {
                int local = bytecode->code[offset++];
                int index = bytecode->code[offset++];

                printf("%04d       |    %-16s %4d\n",
                    offset - 2, local ? "local" : "upvalue", index);
            }
            return offset;
        }

        case OP_RETURN:
            return jml_bytecode_instruction_simple("OP_RETURN", offset);

        case OP_CLASS:
            return jml_bytecode_instruction_const("OP_CLASS", bytecode, offset);

        case EXTENDED_OP(OP_CLASS):
            return jml_bytecode_instruction_const_extended("OP_CLASS_EXTENDED", bytecode, offset);

        case OP_CLASS_FIELD:
            return jml_bytecode_instruction_const("OP_CLASS_FIELD", bytecode, offset);

        case EXTENDED_OP(OP_CLASS_FIELD):
            return jml_bytecode_instruction_const_extended("OP_CLASS_FIELD_EXTENDED", bytecode, offset);

        case OP_INHERIT:
            return jml_bytecode_instruction_simple("OP_INHERIT", offset);

        case OP_SET_LOCAL:
            return jml_bytecode_instruction_byte("OP_SET_LOCAL", bytecode, offset);

        case EXTENDED_OP(OP_SET_LOCAL):
            return jml_bytecode_instruction_short("OP_SET_LOCAL_EXTENDED", bytecode, offset);

        case OP_GET_LOCAL:
            return jml_bytecode_instruction_byte("OP_GET_LOCAL", bytecode, offset);

        case EXTENDED_OP(OP_GET_LOCAL):
            return jml_bytecode_instruction_short("OP_GET_LOCAL_EXTENDED", bytecode, offset);

        case OP_SET_UPVALUE:
            return jml_bytecode_instruction_byte("OP_SET_UPVALUE", bytecode, offset);

        case EXTENDED_OP(OP_SET_UPVALUE):
            return jml_bytecode_instruction_short("OP_SET_UPVALUE_EXTENDED", bytecode, offset);

        case OP_GET_UPVALUE:
            return jml_bytecode_instruction_byte("OP_GET_UPVALUE", bytecode, offset);

        case EXTENDED_OP(OP_GET_UPVALUE):
            return jml_bytecode_instruction_short("OP_GET_UPVALUE_EXTENDED", bytecode, offset);

        case OP_CLOSE_UPVALUE:
            return jml_bytecode_instruction_simple("OP_CLOSE_UPVALUE", offset);

        case OP_SET_GLOBAL:
            return jml_bytecode_instruction_const("OP_SET_GLOBAL", bytecode, offset);

        case EXTENDED_OP(OP_SET_GLOBAL):
            return jml_bytecode_instruction_const_extended("OP_SET_GLOBAL_EXTENDED", bytecode, offset);

        case OP_GET_GLOBAL:
            return jml_bytecode_instruction_const("OP_GET_GLOBAL", bytecode, offset);

        case EXTENDED_OP(OP_GET_GLOBAL):
            return jml_bytecode_instruction_const_extended("OP_GET_GLOBAL_EXTENDED", bytecode, offset);

        case OP_DEF_GLOBAL:
            return jml_bytecode_instruction_const("OP_DEF_GLOBAL", bytecode, offset);

        case EXTENDED_OP(OP_DEF_GLOBAL):
            return jml_bytecode_instruction_const_extended("OP_DEF_GLOBAL_EXTENDED", bytecode, offset);

        case OP_DEL_GLOBAL:
            return jml_bytecode_instruction_const("OP_DEL_GLOBAL", bytecode, offset);

        case EXTENDED_OP(OP_DEL_GLOBAL):
            return jml_bytecode_instruction_const_extended("OP_DEL_GLOBAL_EXTENDED", bytecode, offset);

        case OP_SET_MEMBER:
            return jml_bytecode_instruction_const("OP_SET_MEMBER", bytecode, offset);

        case EXTENDED_OP(OP_SET_MEMBER):
            return jml_bytecode_instruction_const_extended("OP_SET_MEMBER_EXTENDED", bytecode, offset);

        case OP_GET_MEMBER:
            return jml_bytecode_instruction_const("OP_GET_MEMBER", bytecode, offset);

        case EXTENDED_OP(OP_GET_MEMBER):
            return jml_bytecode_instruction_const_extended("OP_GET_MEMBER_EXTENDED", bytecode, offset);

        case OP_SET_INDEX:
            return jml_bytecode_instruction_simple("OP_SET_INDEX", offset);

        case OP_GET_INDEX:
            return jml_bytecode_instruction_simple("OP_GET_INDEX", offset);

        case OP_SWAP_GLOBAL:
            return jml_bytecode_instruction_consts("OP_SWAP_GLOBAL", bytecode, offset);

        case EXTENDED_OP(OP_SWAP_GLOBAL):
            return jml_bytecode_instruction_consts_extended("OP_SWAP_GLOBAL_EXTENDED", bytecode, offset);

        case OP_SWAP_LOCAL:
            return jml_bytecode_instruction_consts("OP_SWAP_LOCAL", bytecode, offset);

        case OP_SUPER:
            return jml_bytecode_instruction_const("OP_SUPER", bytecode, offset);

        case EXTENDED_OP(OP_SUPER):
            return jml_bytecode_instruction_const_extended("OP_SUPER_EXTENDED", bytecode, offset);

        case OP_ARRAY:
            return jml_bytecode_instruction_byte("OP_ARRAY", bytecode, offset);

        case EXTENDED_OP(OP_ARRAY):
            return jml_bytecode_instruction_short("OP_ARRAY_EXTENDED", bytecode, offset);

        case OP_MAP:
            return jml_bytecode_instruction_byte("OP_MAP", bytecode, offset);

        case EXTENDED_OP(OP_MAP):
            return jml_bytecode_instruction_short("OP_MAP_EXTENDED", bytecode, offset);

        case OP_IMPORT:
            return jml_bytecode_instruction_consts("OP_IMPORT", bytecode, offset);

        case EXTENDED_OP(OP_IMPORT):
            return jml_bytecode_instruction_consts_extended("OP_IMPORT_EXTENDED", bytecode, offset);

        case OP_IMPORT_WILDCARD:
            return jml_bytecode_instruction_consts("OP_IMPORT_WILDCARD", bytecode, offset);

        case EXTENDED_OP(OP_IMPORT_WILDCARD):
            return jml_bytecode_instruction_consts_extended("OP_IMPORT_WILDCARD_EXTENDED", bytecode, offset);

        case OP_END:
            return jml_bytecode_instruction_simple("OP_END", offset);

        default:
            printf("unknown opcode %d\n", instruction);
            return offset + 1;
    }
}


uint32_t
jml_bytecode_instruction_offset(jml_bytecode_t *bytecode, uint32_t offset)
{
    switch (bytecode->code[offset]) {
        case OP_NOP:
        case OP_POP:
        case OP_POP_TWO:
        case OP_ROT:
        case OP_SAVE:
        case OP_NONE:
        case OP_TRUE:
        case OP_FALSE:
        case OP_BOOL:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_POW:
        case OP_DIV:
        case OP_MOD:
        case OP_NOT:
        case OP_NEG:
        case OP_EQUAL:
        case OP_GREATER:
        case OP_GREATEREQ:
        case OP_LESS:
        case OP_LESSEQ:
        case OP_NOTEQ:
        case OP_CONCAT:
        case OP_CONTAIN:
        case OP_RETURN:
        case OP_INHERIT:
        case OP_CLOSE_UPVALUE:
        case OP_SET_INDEX:
        case OP_GET_INDEX:
        case OP_END:
            return 1;

        case OP_CONST:
        case OP_CLASS:
        case OP_CLASS_FIELD:
        case OP_SET_GLOBAL:
        case OP_GET_GLOBAL:
        case OP_DEF_GLOBAL:
        case OP_DEL_GLOBAL:
        case OP_SET_MEMBER:
        case OP_GET_MEMBER:
        case OP_SUPER:
        case OP_CALL:
        case OP_SET_LOCAL:
        case OP_GET_LOCAL:
        case OP_SET_UPVALUE:
        case OP_GET_UPVALUE:
        case OP_ARRAY:
        case OP_MAP:
            return 2;

        case OP_SWAP_GLOBAL:
        case OP_SWAP_LOCAL:
        case OP_IMPORT:
        case OP_IMPORT_WILDCARD:
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_LOOP:
        case OP_INVOKE:
        case OP_SUPER_INVOKE:
        case EXTENDED_OP(OP_CONST):
        case EXTENDED_OP(OP_CLASS):
        case EXTENDED_OP(OP_CLASS_FIELD):
        case EXTENDED_OP(OP_SET_GLOBAL):
        case EXTENDED_OP(OP_GET_GLOBAL):
        case EXTENDED_OP(OP_DEF_GLOBAL):
        case EXTENDED_OP(OP_DEL_GLOBAL):
        case EXTENDED_OP(OP_SET_MEMBER):
        case EXTENDED_OP(OP_GET_MEMBER):
        case EXTENDED_OP(OP_SWAP_GLOBAL):
        case EXTENDED_OP(OP_SUPER):
        case EXTENDED_OP(OP_SET_LOCAL):
        case EXTENDED_OP(OP_GET_LOCAL):
        case EXTENDED_OP(OP_SET_UPVALUE):
        case EXTENDED_OP(OP_GET_UPVALUE):
        case EXTENDED_OP(OP_ARRAY):
        case EXTENDED_OP(OP_MAP):
            return 3;

        case EXTENDED_OP(OP_INVOKE):
        case EXTENDED_OP(OP_SUPER_INVOKE):
        case EXTENDED_OP(OP_IMPORT):
        case EXTENDED_OP(OP_IMPORT_WILDCARD):
            return 5;

        case OP_CLOSURE: {
            uint32_t out        = 1;
            uint8_t constant    = bytecode->code[out++];

            jml_obj_function_t *function = AS_FUNCTION(
                bytecode->constants.values[constant]);
            out                 += 2 * function->upvalue_count;
            return out;
        }

        case EXTENDED_OP(OP_CLOSURE): {
            uint32_t out        = 1;
            uint16_t constant   = (bytecode->code[out++] << 8);
            constant            |= bytecode->code[out++];

            jml_obj_function_t *function = AS_FUNCTION(
                bytecode->constants.values[constant]);
            out                 += 2 * function->upvalue_count;
            return out;
        }

        default:
            return 1;
    }
}
