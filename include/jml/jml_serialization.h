#ifndef JML_SERIALIZATION_H_
#define JML_SERIALIZATION_H_

#include <jml.h>
#include <jml/jml_bytecode.h>

size_t jml_serialize_obj(jml_obj_t *obj, uint8_t *serial,
    size_t *size, size_t pos);

size_t jml_serialize_value(jml_value_t value, uint8_t *serial,
    size_t *size, size_t pos);

uint8_t *jml_serialize_bytecode(jml_bytecode_t *bytecode, size_t *length);

bool jml_serialize_bytecode_file(jml_bytecode_t *bytecode,
    const char *filename);


bool jml_deserialize_obj(uint8_t *serial, size_t length,
    size_t *pos, jml_value_t *value);

bool jml_deserialize_value(uint8_t *serial, size_t length,
    size_t *pos, jml_value_t *value);

bool jml_deserialize_bytecode(uint8_t *serial,
    size_t length, jml_bytecode_t *bytecode);

bool jml_deserialize_bytecode_file(jml_bytecode_t *bytecode,
    const char *filename);


#endif /* JML_SERIALIZATION_H_ */
