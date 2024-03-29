#ifndef JML_BYTECODE_H_
#define JML_BYTECODE_H_

#include <jml/jml_common.h>
#include <jml/jml_value.h>


#define EXTENDED_OP(op)             op ## _EXTENDED


typedef enum {
    OP_NOP,
    OP_POP,
    OP_POP_TWO,
    OP_ROT,
    OP_SAVE,
    OP_CONST,
    EXTENDED_OP(OP_CONST),
    OP_NONE,
    OP_TRUE,
    OP_FALSE,
    OP_BOOL,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_POW,
    OP_DIV,
    OP_MOD,
    OP_NOT,
    OP_NEG,
    OP_EQUAL,
    OP_GREATER,
    OP_GREATEREQ,
    OP_LESS,
    OP_LESSEQ,
    OP_NOTEQ,
    OP_CONCAT,
    OP_CONTAIN,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_TRY_CALL,
    OP_INVOKE,
    EXTENDED_OP(OP_INVOKE),
    OP_TRY_INVOKE,
    EXTENDED_OP(OP_TRY_INVOKE),
    OP_TRY_SUPER_INVOKE,
    EXTENDED_OP(OP_TRY_SUPER_INVOKE),
    OP_SUPER_INVOKE,
    EXTENDED_OP(OP_SUPER_INVOKE),
    OP_CLOSURE,
    EXTENDED_OP(OP_CLOSURE),
    OP_RETURN,
    OP_SPREAD,
    OP_CLASS,
    EXTENDED_OP(OP_CLASS),
    OP_CLASS_FIELD,
    EXTENDED_OP(OP_CLASS_FIELD),
    OP_INHERIT,
    OP_SET_LOCAL,
    EXTENDED_OP(OP_SET_LOCAL),
    OP_GET_LOCAL,
    EXTENDED_OP(OP_GET_LOCAL),
    OP_SET_UPVALUE,
    EXTENDED_OP(OP_SET_UPVALUE),
    OP_GET_UPVALUE,
    EXTENDED_OP(OP_GET_UPVALUE),
    OP_CLOSE_UPVALUE,
    OP_SET_GLOBAL,
    EXTENDED_OP(OP_SET_GLOBAL),
    OP_GET_GLOBAL,
    EXTENDED_OP(OP_GET_GLOBAL),
    OP_DEF_GLOBAL,
    EXTENDED_OP(OP_DEF_GLOBAL),
    OP_DEL_GLOBAL,
    EXTENDED_OP(OP_DEL_GLOBAL),
    OP_SET_MEMBER,
    EXTENDED_OP(OP_SET_MEMBER),
    OP_GET_MEMBER,
    EXTENDED_OP(OP_GET_MEMBER),
    OP_SET_INDEX,
    OP_GET_INDEX,
    OP_SWAP_GLOBAL,
    EXTENDED_OP(OP_SWAP_GLOBAL),
    OP_SWAP_LOCAL,
    OP_SUPER,
    EXTENDED_OP(OP_SUPER),
    OP_ARRAY,
    EXTENDED_OP(OP_ARRAY),
    OP_MAP,
    EXTENDED_OP(OP_MAP),
    OP_IMPORT,
    EXTENDED_OP(OP_IMPORT),
    OP_IMPORT_WILDCARD,
    EXTENDED_OP(OP_IMPORT_WILDCARD),
    OP_END
} jml_bytecode_op;


typedef struct {
    uint32_t                        count;
    uint32_t                        capacity;
    uint8_t                        *code;
    uint16_t                       *lines;
    jml_value_array_t               constants;
} jml_bytecode_t;


void jml_bytecode_init(jml_bytecode_t *bytecode);

void jml_bytecode_write(jml_bytecode_t *bytecode,
    uint8_t byte, uint16_t line);

void jml_bytecode_free(jml_bytecode_t *bytecode);

int jml_bytecode_add_const(jml_bytecode_t *bytecode,
    jml_value_t value);


void jml_bytecode_disassemble(jml_bytecode_t *bytecode,
    const char *name);

uint32_t jml_bytecode_instruction_disassemble(
    jml_bytecode_t *bytecode, uint32_t offset);

uint32_t jml_bytecode_instruction_offset(
    jml_bytecode_t *bytecode, uint32_t offset);


#endif /* JML_BYTECODE_H_ */
