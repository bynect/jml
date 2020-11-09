#ifndef _JML_BYTECODE_H_
#define _JML_BYTECODE_H_

#include <jml_common.h>
#include <jml_value.h>


typedef enum {
    OP_POP,
    OP_POP_TWO,
    OP_ROT,
    OP_CONST,
    OP_NONE,
    OP_TRUE,
    OP_FALSE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_POW,
    OP_MOD,
    OP_NOT,
    OP_NEGATE,
    OP_EQUAL,
    OP_GREATER,
    OP_GREATEREQ,
    OP_LESS,
    OP_LESSEQ,
    OP_NOTEQ,
    OP_JMP,
    OP_JMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_METHOD,
    OP_INVOKE,
    OP_SUPER_INVOKE,
    OP_CLOSURE,
    OP_RETURN,
    OP_CLASS,
    OP_INHERIT,
    OP_SET_LOCAL,
    OP_GET_LOCAL,
    OP_SET_UPVALUE,
    OP_GET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_SET_GLOBAL,
    OP_GET_GLOBAL,
    OP_DEF_GLOBAL,
    OP_SET_PROPERTY,
    OP_GET_PROPERTY,
    OP_SUPER,
} jml_bytecode_op;


typedef struct {
    int                             count;
    int                             capacity;
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

int jml_bytecode_instruction_disassemble(jml_bytecode_t *bytecode,
    int offset);


#endif /* _JML_BYTECODE_H_ */
