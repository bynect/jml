#ifndef JML_SERIALIZATION_H_
#define JML_SERIALIZATION_H_

#include <jml.h>
#include <jml_bytecode.h>


uint8_t *jml_bytecode_serialize(jml_bytecode_t *bytecode, size_t *length);

bool jml_bytecode_serialize_file(jml_bytecode_t *bytecode, const char *filename);


bool jml_bytecode_deserialize(uint8_t *serial, size_t length, jml_bytecode_t *bytecode);

bool jml_bytecode_deserialize_file(jml_bytecode_t *bytecode, char *filename);


#endif /* JML_SERIALIZATION_H_ */
