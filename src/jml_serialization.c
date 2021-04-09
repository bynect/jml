#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <jml/jml_serialization.h>
#include <jml/jml_gc.h>


#define JML_SHEBANG                 "#!/usr/bin/env -S jml -b\n"
#define JML_MAGIC                   "jml"

#define JML_SERIAL_NUM              'N'
#define JML_SERIAL_OBJ              'O'
#define JML_SERIAL_STRING           'S'

#define JML_SERIAL_NONE             '|'
#define JML_SERIAL_TRUE             '<'
#define JML_SERIAL_FALSE            '>'


size_t
jml_serialize_short(uint16_t num, uint8_t *serial,
    size_t *size, size_t pos)
{
    REALLOC(uint8_t, serial, *size, pos + sizeof(uint16_t));

    serial[pos + 0] = (num >> 8) & 0xff;
    serial[pos + 1] = num & 0xff;

    return sizeof(uint16_t);
}


size_t
jml_serialize_long(uint32_t num,uint8_t *serial,
    size_t *size, size_t pos)
{
    REALLOC(uint8_t, serial, *size, pos + sizeof(uint32_t));

    serial[pos + 0] = (num >> 24) & 0xff;
    serial[pos + 1] = (num >> 16) & 0xff;
    serial[pos + 2] = (num >> 8) & 0xff;
    serial[pos + 3] = num & 0xff;

    return sizeof(uint32_t);
}


size_t
jml_serialize_longlong(uint64_t num, uint8_t *serial,
    size_t *size, size_t pos)
{
    REALLOC(uint8_t, serial, *size, pos + sizeof(uint64_t));

    serial[pos + 0] = (num >> 56) & 0xff;
    serial[pos + 1] = (num >> 48) & 0xff;
    serial[pos + 2] = (num >> 40) & 0xff;
    serial[pos + 3] = (num >> 32) & 0xff;
    serial[pos + 4] = (num >> 24) & 0xff;
    serial[pos + 5] = (num >> 16) & 0xff;
    serial[pos + 6] = (num >> 8) & 0xff;
    serial[pos + 7] = num & 0xff;

    return sizeof(uint64_t);
}


size_t
jml_serialize_double(double num, uint8_t *serial,
    size_t *size, size_t pos)
{
    return jml_serialize_longlong(*(uint64_t*)&num, serial, size, pos);
}


size_t
jml_serialize_string(jml_obj_string_t *string,
    uint8_t *serial, size_t *size, size_t pos)
{
    size_t posx = pos;
    posx += jml_serialize_long(string->length, serial, size, posx);

    REALLOC(uint8_t, serial, *size, posx + string->length);
    memcpy(serial + posx, string->chars, string->length);
    posx += string->length;

    return posx - pos;
}


size_t
jml_serialize_obj(jml_value_t value,
    uint8_t *serial, size_t *size, size_t pos)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: {
            size_t posx = pos;

            REALLOC(uint8_t, serial, *size, posx + 2);
            posx += snprintf((char*)serial + posx, *size - posx, "%c%c",
                JML_SERIAL_OBJ, JML_SERIAL_STRING
            );

            posx += jml_serialize_string(AS_STRING(value), serial, size, posx);
            return posx - pos;
        }

        default:
            break;
    }

    return 0;
}


size_t
jml_serialize_value(jml_value_t value,
    uint8_t *serial, size_t *size, size_t pos)
{
#ifdef JML_NAN_TAGGING
    if (IS_OBJ(value))
        return jml_serialize_obj(value, serial, size, pos);

    else if (IS_NUM(value)) {
        REALLOC(uint8_t, serial, *size, pos + 1);
        snprintf((char*)serial + pos, *size - pos, "%c", JML_SERIAL_NUM);

        double num = AS_NUM(value);
        return 1 + jml_serialize_double(num, serial, size, pos + 1);

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
            REALLOC(uint8_t, serial, *size, pos + 1);
            snprintf((char*)serial + pos, *size - pos, "%c", JML_SERIAL_NUM);

            double num = AS_NUM(value);
            return 1 + jml_serialize_double(num, serial, size, pos + 1);
        }

        case VAL_OBJ:
            return jml_serialize_obj(AS_OBJ(value), serial, size, pos);
    }
#endif
    return 0;
}


uint8_t *
jml_serialize_bytecode(jml_bytecode_t *bytecode, size_t *length)
{
    size_t size         = SERIAL_MIN;
    size_t pos          = 0;
    uint32_t offset     = 0;
    uint8_t *serial     = jml_realloc(NULL, size);

    /*shebang*/
    pos += snprintf((char*)serial, size, "%s", JML_SHEBANG);

    /*magic*/
    pos += snprintf((char*)serial + pos, size - pos, "%s%c%c%c", JML_MAGIC,
        JML_VERSION_MAJOR, JML_VERSION_MINOR, JML_VERSION_MICRO
    );

    /*values offset*/
    offset = bytecode->constants.count;
    pos += jml_serialize_long(offset, serial, &size, pos);

    offset = bytecode->count * (sizeof(uint16_t) + 1);
    pos += jml_serialize_long(offset, serial, &size, pos);

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
        pos += jml_serialize_value(
            bytecode->constants.values[i], serial, &size, pos
        );
    }

    if (length != NULL)
        *length = pos;

    return serial;
}


bool
jml_serialize_bytecode_file(jml_bytecode_t *bytecode, const char *filename)
{
    FILE *file = fopen(filename, "wb");
    if (file == NULL) return false;

    size_t length = 0;
    uint8_t *serial = jml_serialize_bytecode(bytecode, &length);

    if (fwrite(serial, sizeof(uint8_t), length, file) < length) {
        fclose(file);
        jml_free(serial);
        return false;
    }

    fclose(file);
    jml_free(serial);
    return true;
}


bool
jml_deserialize_short(uint8_t *serial, size_t length,
    size_t *pos, uint16_t *num)
{
    if (length < (*pos + sizeof(uint16_t)))
        return false;

    *num = (uint16_t)serial[*pos + 0] << 8;
    *num |= (uint16_t)serial[*pos + 1];

    *pos += sizeof(uint16_t);
    return true;
}


bool
jml_deserialize_long(uint8_t *serial, size_t length,
    size_t *pos, uint32_t *num)
{
    if (length < (*pos + sizeof(uint32_t)))
        return false;

    *num = (uint32_t)serial[*pos + 0] << 24;
    *num += (uint32_t)serial[*pos + 1] << 16;
    *num += (uint32_t)serial[*pos + 2] << 8;
    *num += (uint32_t)serial[*pos + 3];

    *pos += sizeof(uint32_t);
    return true;
}


bool
jml_deserialize_longlong(uint8_t *serial, size_t length,
    size_t *pos, uint64_t *num)
{
    if (length < (*pos + sizeof(uint64_t)))
        return false;

    *num = (uint64_t)serial[*pos + 0] << 56;
    *num += (uint64_t)serial[*pos + 1] << 48;
    *num += (uint64_t)serial[*pos + 2] << 40;
    *num += (uint64_t)serial[*pos + 3] << 32;
    *num += (uint64_t)serial[*pos + 4] << 24;
    *num += (uint64_t)serial[*pos + 5] << 16;
    *num += (uint64_t)serial[*pos + 6] << 8;
    *num += (uint64_t)serial[*pos + 7];

    *pos += sizeof(uint64_t);
    return true;
}


bool
jml_deserialize_double(uint8_t *serial, size_t length,
    size_t *pos, double *num)
{
    return jml_deserialize_longlong(serial, length, pos, (uint64_t*)num);
}


bool
jml_deserialize_string(uint8_t *serial, size_t length,
    size_t *pos, jml_obj_string_t **string)
{
    uint32_t size = 0;

    if (!jml_deserialize_long(serial, length, pos, &size))
        return false;

    if (length < (*pos + size))
        return false;

    char *buffer = jml_realloc(NULL, size + 1);
    memcpy(buffer, serial + *pos, size);
    buffer[size] = '\0';
    *pos += size;

    *string = jml_obj_string_take(buffer, size);
}


bool
jml_deserialize_obj(uint8_t *serial, size_t length,
    size_t *pos, jml_value_t *value)
{
    if (length <= *pos)
        return false;

    uint8_t byte = serial[(*pos)++];
    switch (byte) {
        case JML_SERIAL_STRING: {
            jml_obj_string_t *string;
            if (!jml_deserialize_string(serial, length, pos, &string))
                return false;

            *value = OBJ_VAL(string);
            return true;
        }

        case JML_SERIAL_MODULE: {
            break;
        }

        case JML_SERIAL_FUNCTION: {
            break;
        }

        default:
            break;
    }

    return false;
}


bool
jml_deserialize_value(uint8_t *serial, size_t length,
    size_t *pos, jml_value_t *value)
{
    if (length <= *pos)
        return false;

    uint8_t byte = serial[(*pos)++];
    switch (byte) {
        case JML_SERIAL_NUM: {
            double num;
            if (!jml_deserialize_double(serial, length, pos, &num))
                return false;

            *value = NUM_VAL(num);
            break;
        }

        case JML_SERIAL_OBJ:
            return jml_deserialize_obj(serial, length, pos, value);

        case JML_SERIAL_NONE: {
            *value = NONE_VAL;
            break;
        }

        case JML_SERIAL_TRUE:
        case JML_SERIAL_FALSE: {
            *value = BOOL_VAL(byte == JML_SERIAL_TRUE);
            break;
        }

        default:
            return false;
    }
    return true;
}


bool
jml_deserialize_bytecode(uint8_t *serial, size_t length, jml_bytecode_t *bytecode)
{
    jml_bytecode_init(bytecode);
    size_t shebang_length   = strlen(JML_SHEBANG);
    size_t pos              = 0;
    uint32_t offset         = 0;
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
    if (!jml_deserialize_long(serial, length, &pos, &constants))
        goto err;

    if (!jml_deserialize_long(serial, length, &pos, &offset))
        goto err;

    count = offset / (sizeof(uint16_t) + 1);

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
        jml_value_t value;
        if (!jml_deserialize_value(serial, length, &pos, &value))
            goto err;

        jml_value_array_write(&bytecode->constants, value);
    }

    return true;

err:
    jml_bytecode_free(bytecode);
    return false;
}


bool
jml_deserialize_bytecode_file(jml_bytecode_t *bytecode, const char *filename)
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
    bool result = jml_deserialize_bytecode(serial, size, bytecode);

    fclose(file);
    jml_free(serial);
    return result;
}
