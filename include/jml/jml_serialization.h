#ifndef JML_SERIALIZATION_H_
#define JML_SERIALIZATION_H_

#include <jml.h>
#include <jml/jml_bytecode.h>


#define JML_SHEBANG                 "#!/usr/bin/env -S jml -b\n"
#define JML_MAGIC                   "jml"

#define JML_SERIAL_NUM              'N'
#define JML_SERIAL_OBJ              'O'
#define JML_SERIAL_STRING           'S'
#define JML_SERIAL_MODULE           'M'
#define JML_SERIAL_FUNCTION         'F'

#define JML_SERIAL_NONE             '|'
#define JML_SERIAL_TRUE             '<'
#define JML_SERIAL_FALSE            '>'


/*serialization*/
size_t jml_serialize_short(uint16_t num, uint8_t *serial,
    size_t *size, size_t pos);

size_t jml_serialize_long(uint32_t num,uint8_t *serial,
    size_t *size, size_t pos);

size_t jml_serialize_longlong(uint64_t num, uint8_t *serial,
    size_t *size, size_t pos);

size_t jml_serialize_double(double num, uint8_t *serial,
    size_t *size, size_t pos);

size_t jml_serialize_string(jml_obj_string_t *string,
    uint8_t *serial, size_t *size, size_t pos);

size_t jml_serialize_obj(jml_value_t value, uint8_t *serial,
    size_t *size, size_t pos);

size_t jml_serialize_value(jml_value_t value, uint8_t *serial,
    size_t *size, size_t pos);

size_t jml_serialize_bytecode(jml_bytecode_t *bytecode,
    uint8_t *serial, size_t *size, size_t pos);

bool jml_serialize_bytecode_file(jml_bytecode_t *bytecode,
    const char *filename);


/*deserialization*/
bool jml_deserialize_short(uint8_t *serial, size_t length,
    size_t *pos, uint16_t *num);

bool jml_deserialize_long(uint8_t *serial, size_t length,
    size_t *pos, uint32_t *num);

bool jml_deserialize_longlong(uint8_t *serial, size_t length,
    size_t *pos, uint64_t *num);

bool jml_deserialize_double(uint8_t *serial, size_t length,
    size_t *pos, double *num);

bool jml_deserialize_string(uint8_t *serial, size_t length,
    size_t *pos, jml_obj_string_t **string);

bool jml_deserialize_obj(uint8_t *serial, size_t length,
    size_t *pos, jml_value_t *value);

bool jml_deserialize_value(uint8_t *serial, size_t length,
    size_t *pos, jml_value_t *value);

bool jml_deserialize_bytecode(uint8_t *serial, size_t length,
    size_t *pos, jml_bytecode_t *bytecode);

bool jml_deserialize_bytecode_file(jml_bytecode_t *bytecode,
    const char *filename);


#endif /* JML_SERIALIZATION_H_ */
