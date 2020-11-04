#include <jml_common.h>
#include <jml_value.h>
#include <jml_bytecode.h>
#include <jml_vm.h>


void
jml_bytecode_init(jml_bytecode_t *bytecode)
{
    bytecode->count = 0;
    bytecode->code = NULL;
    bytecode->lines = NULL;
    bytecode->capacity = 0;
    jml_value_array_free(&(bytecode->constants));
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
jml_bytecode_free(jml_bytecode_t *bytecode) {
    FREE_ARRAY(uint8_t, bytecode->code, bytecode->capacity);
    FREE_ARRAY(int, bytecode->lines, bytecode->capacity);
    jml_value_array_free(&(bytecode->constants));
    jml_bytecode_init(bytecode);
}


int
jml_bytecode_add_const(jml_bytecode_t *bytecode, jml_value_t value) {
    jml_vm_push(value);
    jml_value_array_write(&(bytecode->constants), value);
    jml_vm_pop();
    return bytecode->constants.count - 1;
}
