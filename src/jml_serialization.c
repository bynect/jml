#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <jml_serialization.h>
#include <jml_gc.h>


#define JML_SHEBANG                 "#!/usr/bin/env -S jml -b\n"
#define JML_MAGIC                   "jml"

#define JML_SERIAL_NUM              'N'
#define JML_SERIAL_OBJ              'O'
#define JML_SERIAL_NONE             '|'
#define JML_SERIAL_TRUE             '<'
#define JML_SERIAL_FALSE            '>'


static size_t
jml_bytecode_serialize_obj(jml_obj_t *obj,
    uint8_t *serial, size_t *size, size_t pos)
{
    (void) obj;
    (void) serial;
    (void) size;
    (void) pos;
    return 0;
}


static size_t
jml_bytecode_serialize_value(jml_value_t value,
    uint8_t *serial, size_t *size, size_t pos)
{
#ifdef JML_NAN_TAGGING
    if (IS_OBJ(value))
        return jml_bytecode_serialize_obj(AS_OBJ(value), serial, size, pos);

    else if (IS_NUM(value)) {
        size_t posx = pos;
        double num = AS_NUM(value);

        REALLOC(uint8_t, serial, *size, pos + sizeof(double) + 1);
        posx += snprintf((char*)serial + posx, *size - posx, "%c", JML_SERIAL_NUM);

        for (uint8_t i = 0; i < sizeof(double); ++i)
            posx += snprintf((char*)serial + posx, *size - posx, "%c", ((uint8_t*)&num)[i]);

        return posx - pos;

    } else if (IS_BOOL(value)) {
        REALLOC(uint8_t, serial, *size, pos + 1);
        return snprintf((char*)serial + pos, *size - pos, "%c",
            AS_BOOL(value) ? JML_SERIAL_TRUE : JML_SERIAL_FALSE
        );

    } else if (IS_NONE(value)) {
        REALLOC(uint8_t, serial, *size, pos + 1);
        return snprintf((char*)serial + pos, *size - pos, "%c", JML_SERIAL_NONE);
    }
#else
    switch (value.type) {
        case VAL_BOOL: {
            REALLOC(uint8_t, serial, *size, pos + 1);
            return snprintf((char*)serial + pos, *size - pos, "%c",
                AS_BOOL(value) ? JML_SERIAL_TRUE : JML_SERIAL_FALSE
            );
        }

        case VAL_NONE: {
            REALLOC(uint8_t, serial, *size, pos + 1);
            return snprintf((char*)serial + pos, *size - pos, "%c", JML_SERIAL_NONE);
        }

        case VAL_NUM: {
            size_t posx = pos;
            double num = AS_NUM(value);

            REALLOC(uint8_t, serial, *size, pos + sizeof(double) + 1);
            posx += snprintf((char*)serial + posx, *size - posx, "%c", JML_SERIAL_NUM);

            for (uint8_t i = 0; i < sizeof(double); ++i)
                posx += snprintf((char*)serial + posx, *size - posx, "%c", ((uint8_t*)&num)[i]);

            return posx - pos;
        }

        case VAL_OBJ:
            return jml_bytecode_serialize_obj(AS_OBJ(value), serial, size, pos);
    }
#endif
    return 0;
}


uint8_t *
jml_bytecode_serialize(jml_bytecode_t *bytecode, size_t *length)
{
    size_t size         = SERIAL_MIN;
    size_t pos          = 0;
    size_t offset       = 0;
    uint8_t *serial     = jml_realloc(NULL, size);

    /*shebang*/
    pos += snprintf((char*)serial, size, "%s", JML_SHEBANG);

    /*magic*/
    pos += snprintf((char*)serial + pos, size - pos, "%s%c%c%c", JML_MAGIC,
        JML_VERSION_MAJOR, JML_VERSION_MINOR, JML_VERSION_MICRO
    );

    /*values offset*/
    offset = bytecode->constants.count;
    for (uint8_t i = 0; i < sizeof(uint32_t); ++i)
        pos += snprintf((char*)serial + pos, size - pos, "%c", ((uint8_t*)&offset)[i]);

    offset = bytecode->count * (sizeof(uint16_t) + 1);
    for (uint8_t i = 0; i < sizeof(uint32_t); ++i)
        pos += snprintf((char*)serial + pos, size - pos, "%c", ((uint8_t*)&offset)[i]);

    /*opcodes*/
    offset = bytecode->count;
    REALLOC(uint8_t, serial, size, pos + offset);
    memcpy(serial + pos, bytecode->code, offset);
    pos += offset;

    /*lines*/
    offset = bytecode->count * sizeof(uint16_t);
    REALLOC(uint8_t, serial, size, pos + offset);
    memcpy(serial + pos, bytecode->lines, offset);
    pos += offset;

    /*values*/
    for (int i = 0; i < bytecode->constants.count; ++i) {
        pos += jml_bytecode_serialize_value(
            bytecode->constants.values[i], serial, &size, pos
        );
    }

    pos += snprintf((char*)serial + pos, size - pos, "\n");

    if (length != NULL)
        *length = pos;

    return serial;
}


bool
jml_bytecode_serialize_file(jml_bytecode_t *bytecode, const char *filename)
{
    FILE *file = fopen(filename, "wb");
    if (file == NULL) return false;

    size_t length = 0;
    uint8_t *serial = jml_bytecode_serialize(bytecode, &length);

    if (fwrite(serial, sizeof(uint8_t), length, file) < length) {
        fclose(file);
        jml_free(serial);
        return false;
    }

    fclose(file);
    jml_free(serial);
    return true;
}


static bool
jml_bytecode_deserialize_value(uint8_t *serial, size_t length,
    size_t *pos, jml_bytecode_t *bytecode)
{
    (void) serial;
    (void) length;
    (void) pos;
    (void) bytecode;
    return false;
}


static bool
jml_bytecode_deserialize_value(uint8_t *serial, size_t length,
    size_t *pos, jml_bytecode_t *bytecode)
{
    uint8_t byte = serial[(*pos)++];
    switch (byte) {
        case JML_SERIAL_NUM: {
            if (length < (*pos + sizeof(double))) return false;

            uint8_t bytes[sizeof(double)];
            for (uint8_t i = 0; i < sizeof(double); ++i)
                bytes[i] = serial[*pos + i];

            *pos += sizeof(double);
            double num = *(double*)bytes;
            jml_value_array_write(&bytecode->constants, NUM_VAL(num));
            break;
        }

        case JML_SERIAL_OBJ:
            return jml_bytecode_deserialize_obj(serial, length, pos, bytecode);

        case JML_SERIAL_NONE: {
            jml_value_array_write(&bytecode->constants, NONE_VAL);
            break;
        }

        case JML_SERIAL_TRUE:
        case JML_SERIAL_FALSE: {
            jml_value_array_write(&bytecode->constants, BOOL_VAL(byte == JML_SERIAL_TRUE));
            break;
        }

        default:
            return false;
    }
    return true;
}


bool
jml_bytecode_deserialize(uint8_t *serial, size_t length, jml_bytecode_t *bytecode)
{
    jml_bytecode_init(bytecode);
    size_t shebang_length   = strlen(JML_SHEBANG);
    size_t pos              = 0;
    size_t offset           = 0;
    uint32_t count          = 0;
    uint32_t constants      = 0;

    /*shebang*/
    if (length > shebang_length && memcmp(serial, JML_SHEBANG, shebang_length) == 0)
        pos += shebang_length;

    /*magic*/
    uint8_t magic[] = {
        JML_MAGIC[0], JML_MAGIC[1], JML_MAGIC[2],
        JML_VERSION_MAJOR, JML_VERSION_MINOR, JML_VERSION_MICRO
    };

    if ((length - pos) > sizeof(magic) && memcmp(serial + pos, magic, sizeof(magic)) == 0)
        pos += sizeof(magic);
    else
        return false;

    /*values offset*/
    if ((length - pos) > (sizeof(uint32_t) * 2)) {
        uint8_t bytes[sizeof(uint32_t)];
        for (uint8_t i = 0; i < sizeof(uint32_t); ++i)
            bytes[i] = serial[pos + i];

        constants = *(uint32_t*)bytes;
        pos += sizeof(uint32_t);

        for (uint8_t i = 0; i < sizeof(uint32_t); ++i)
            bytes[i] = serial[pos + i];

        offset = *(uint32_t*)bytes;
        count = offset / (sizeof(uint16_t) + 1);
        pos += sizeof(uint32_t);
    } else
        return false;

    if (length < (offset + pos))
        return false;

    /*opcodes and lines*/
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t line;
        memcpy(&line, &serial[pos + count + (i * 2)], sizeof(uint16_t));
        jml_bytecode_write(bytecode, serial[pos + i], line);
    }
    pos += offset;

    /*values*/
    for (uint32_t i = 0; i < constants; ++i) {
        if (!jml_bytecode_deserialize_value(serial, length, &pos, bytecode))
            goto err;
    }

    return true;

err:
    jml_bytecode_free(bytecode);
    return false;
}


bool
jml_bytecode_deserialize_file(jml_bytecode_t *bytecode, char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) return false;

    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    uint8_t *serial = jml_realloc(NULL, size);
    if (fread(serial, sizeof(uint8_t), size, file) < size) {
        fclose(file);
        jml_free(serial);
        return false;
    }

    bool result = jml_bytecode_deserialize(serial, size, bytecode);

    fclose(file);
    jml_free(serial);
    return result;
}
