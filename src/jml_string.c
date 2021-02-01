#include <jml_string.h>


uint8_t
jml_string_encode(char *buffer, uint32_t value)
{
    char *bytes = buffer;

    if (value <= 0x7f) {
        *bytes = (char)(value & 0x7f);
        return 1;
    }

    if (value <= 0x7ff) {
        *bytes++ = (char)(0xc0 | ((value & 0x7c0) >> 6));
        *bytes = (char)(0x80 | (value & 0x3f));
        return 2;
    }

    if (value <= 0xffff) {
        *bytes++ = (char)(0xe0 | ((value & 0xf000) >> 12));
        *bytes++ = (char)(0x80 | ((value & 0xfc0) >> 6));
        *bytes = (char)(0x80 | (value & 0x3f));
        return 3;
    }

    if (value <= 0x10ffff) {
        *bytes++ = (char)(0xf0 | ((value & 0x1c0000) >> 18));
        *bytes++ = (char)(0x80 | ((value & 0x3f000) >> 12));
        *bytes++ = (char)(0x80 | ((value & 0xfc0) >> 6));
        *bytes = (char)(0x80 | (value & 0x3f));
        return 4;
    }

    return 0;
}


const char *
jml_string_decode(const char *str, uint32_t *val)
{
    static const uint32_t limits[] = {
        ~(uint32_t)0, 0x80, 0x800, 0x10000u, 0x200000u, 0x4000000u
    };

    unsigned int c = (unsigned char)str[0];
    uint32_t res = 0;

    if (c < 0x80)
        res = c;
    else {
        int count = 0;

        for (; c & 0x40; c <<= 1) {
            unsigned int cc = (unsigned char)str[++count];
            if ((cc & 0xC0) != 0x80)
                return NULL;
            res = (res << 6) | (cc & 0x3F);
        }

        res |= ((uint32_t)(c & 0x7F) << (count * 5));

        if (count > 5 || res > JML_MAXUTF || res < limits[count])
            return NULL;

        str += count;
    }

    if (val)
        *val = res;

    return str + 1;
}


uint8_t
jml_string_charbytes(const char *str, uint32_t i)
{
    unsigned char c = (unsigned char)str[i];

    if ((c > 0) && (c <= 127)) return 1;
    if ((c >= 194) && (c <= 223)) return 2;
    if ((c >= 224) && (c <= 239)) return 3;
    if ((c >= 240) && (c <= 244)) return 4;

    return 0;
}


size_t
jml_string_len(const char *str, size_t size)
{
    if (size == 0)
        size = strlen(str);

    size_t pos = 0;
    size_t len = 0;

    while (pos < size) {
        ++len;
        uint32_t n = jml_string_charbytes(str, pos);

        if (n == 0)
            return 0;

        pos += n;
    }

    return len;
}
