#include <stdlib.h>
#include <stdio.h>

#include <jml.h>
#include <jml/jml_gc.h>


/*parser*/
typedef enum {
    ALLOW_TRAILING_COMMA = 0x01,
    ALLOW_UNQUOTED_KEYS = 0x02,
    ALLOW_GLOBAL = 0x04,
    ALLOW_EQUALS = 0x08,
    ALLOW_NO_COMMAS = 0x10,
    ALLOW_COMMENTS = 0x20,
    ALLOW_SINGLE_QUOTED = 0x40,
    ALLOW_HEX_NUMBER = 0x80,
    ALLOW_PLUS_SIGN = 0x100,
    ALLOW_DECIMAL_POINT = 0x200,
    ALLOW_INF_NAN = 0x400,
    ALLOW_MULTILINE_STRING = 0x800,
    JSON_STRICT = 0x00,
    JSON_LENIENT = ALLOW_TRAILING_COMMA | ALLOW_UNQUOTED_KEYS | ALLOW_GLOBAL
        | ALLOW_EQUALS | ALLOW_NO_COMMAS,
    JSON5 = ALLOW_TRAILING_COMMA | ALLOW_UNQUOTED_KEYS | ALLOW_COMMENTS
        | ALLOW_SINGLE_QUOTED | ALLOW_HEX_NUMBER | ALLOW_PLUS_SIGN
        | ALLOW_DECIMAL_POINT | ALLOW_INF_NAN | ALLOW_MULTILINE_STRING
} jml_json_mode;


typedef enum {
    ERROR_NONE = 0,
    ERROR_EXPECTED_CLOSING,
    ERROR_EXPECTED_COLON,
    ERROR_EXPECTED_QUOTE,
    ERROR_INVALID_ESCAPE,
    ERROR_INVALID_NUMBER,
    ERROR_INVALID_VALUE,
    ERROR_INVALID_STRING,
    ERROR_UNEXPECTED_EOF,
    ERROR_UNEXPECTED_CHARS,
    ERROR_UNKNOWN
} jml_json_error;


typedef struct {
    jml_json_error                  error;
    size_t                          off;
    size_t                          line_no;
    size_t                          col_no;
} jml_json_error_t;


typedef struct {
    const char                     *src;
    size_t                          size;
    size_t                          off;
    jml_json_mode                   flags;
    char                           *data;
    size_t                          data_size;
    size_t                          line_no;
    size_t                          line_off;
    jml_json_error                  error;
} jml_json_parser_t;


static inline int
jml_json_hex_digit(char c)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    else if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    else if ('A' <= c && c <= 'F')
        return c - 'A' + 10;

    return -1;
}


static bool
jml_json_hex_value(const char *c, size_t size, uint64_t *result)
{
    const char *ptr;
    int digit;

    if (size > sizeof(uint64_t) * 2)
        return false;

    *result = 0;
    for (ptr = c; (uint64_t)(ptr - c) < size; ++ptr) {
        *result <<= 4;
        digit = jml_json_hex_digit(*ptr);
        if (digit < 0 || digit > 15)
            return false;

        *result |= (uint8_t)digit;
    }

    return true;
}


static bool
jml_json_skip_whitespace(jml_json_parser_t *parser)
{
    size_t offset = parser->off;
    const size_t size = parser->size;
    const char *src = parser->src;

    switch (src[offset]) {
        case ' ':
        case '\r':
        case '\t':
        case '\n':
            break;

        default:
            return false;
    }

    do {
        switch (src[offset]) {
            case ' ':
            case '\r':
            case '\t':
                break;

            case '\n':
                ++parser->line_no;
                parser->line_off = offset;
                break;

            default:
                parser->off = offset;
                return true;
        }

        ++offset;
    } while (offset < size);

    parser->off = offset;
    return true;
}


static bool
jml_json_skip_comments(jml_json_parser_t *parser)
{
    if (parser->src[parser->off] == '/') {
        ++parser->off;

        if (parser->src[parser->off] == '/') {
            ++parser->off;

            while (parser->off < parser->size) {
                switch (parser->src[parser->off]) {
                    case '\n':
                        ++parser->off;
                        ++parser->line_no;
                        parser->line_off = parser->off;
                        return true;

                    default:
                        ++parser->off;
                        break;
                }
            }
            return true;

        } else if (parser->src[parser->off] == '*') {
            ++parser->off;

            while (parser->off + 1 < parser->size) {
                if ((parser->src[parser->off] == '*') &&
                    (parser->src[parser->off + 1] == '/')) {
                    parser->off += 2;
                    return true;

                } else if (parser->src[parser->off] == '\n') {
                    ++parser->line_no;
                    parser->line_off = parser->off;
                }

                ++parser->off;
            }

            return true;
        }
    }

    return false;
}


static bool
jml_json_skip_all(jml_json_parser_t *parser)
{
    bool consumed = false;
    const size_t size = parser->size;

    if (parser->flags & ALLOW_COMMENTS) {
        do {
            if (parser->off == size) {
                parser->error = ERROR_UNEXPECTED_EOF;
                return true;
            }
            consumed = jml_json_skip_whitespace(parser);

            if (parser->off == size) {
                parser->error = ERROR_UNEXPECTED_EOF;
                return true;
            }

            consumed |= jml_json_skip_comments(parser);
        } while (consumed);

    } else {
        do {
            if (parser->off == size) {
                parser->error = ERROR_UNEXPECTED_EOF;
                return true;
            }

            consumed = jml_json_skip_whitespace(parser);
        } while (consumed);
    }

    if (parser->off == size) {
        parser->error = ERROR_UNEXPECTED_EOF;
        return true;
    }

    return false;
}


static bool jml_json_value_size(jml_json_parser_t *parser, bool global);


static bool
jml_json_string_size(jml_json_parser_t *parser)
{
    size_t offset = parser->off;
    const size_t size = parser->size;
    size_t data_size = 0;
    const char *src = parser->src;

    const int single_quoted = src[offset] == '\'';
    const char closing_quote = single_quoted ? '\'' : '"';
    const jml_json_mode flags = parser->flags;
    uint64_t codepoint;
    uint64_t high_surrogate = 0;

    if (src[offset] != '"') {
        if (!((flags & ALLOW_SINGLE_QUOTED) && single_quoted)) {
            parser->error = ERROR_EXPECTED_QUOTE;
            parser->off = offset;
            return true;
        }
    }
    ++offset;

    while ((offset < size) && (src[offset] != closing_quote)) {
        ++data_size;

        switch (src[offset]) {
            case '\0':
            case '\t':
                parser->error = ERROR_INVALID_STRING;
                parser->off = offset;
                return true;

            default:
                break;
        }

        if (src[offset] == '\\') {
            ++offset;

            if (offset == size) {
                parser->error = ERROR_UNEXPECTED_EOF;
                parser->off = offset;
                return true;
            }

            switch (src[offset]) {
                case '"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    ++offset;
                    break;

                case 'u':
                    if (!(offset + 5 < size)) {
                        parser->error = ERROR_INVALID_ESCAPE;
                        parser->off = offset;
                        return true;
                    }

                    codepoint = 0;
                    if (!jml_json_hex_value(&src[offset + 1], 4, &codepoint)) {
                        parser->error = ERROR_INVALID_ESCAPE;
                        parser->off = offset;
                        return true;
                    }

                    if (high_surrogate != 0) {
                        if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                            data_size += 3;
                            high_surrogate = 0;
                        } else {
                            parser->error = ERROR_INVALID_ESCAPE;
                            parser->off = offset;
                            return true;
                        }
                    } else if (codepoint <= 0x7f)
                        data_size += 0;
                    else if (codepoint <= 0x7ff)
                        data_size += 1;
                    else if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        if (offset + 11 > size || src[offset + 5] != '\\' ||
                            src[offset + 6] != 'u') {
                            parser->error = ERROR_INVALID_ESCAPE;
                            parser->off = offset;
                            return true;
                        }
                        high_surrogate = codepoint;
                    } else if (codepoint >= 0xd800 && codepoint <= 0xdfff) {
                        parser->error = ERROR_INVALID_ESCAPE;
                        parser->off = offset;
                        return true;
                    } else
                        data_size += 2;

                    offset += 5;
                    break;

                default:
                    parser->error = ERROR_INVALID_ESCAPE;
                    parser->off = offset;
                    return true;
            }
        } else if ((src[offset] == '\r') || (src[offset] == '\n')) {
            if (!(flags & ALLOW_MULTILINE_STRING)) {
                parser->error = ERROR_INVALID_ESCAPE;
                parser->off = offset;
                return true;
            }
            ++offset;
        } else
            ++offset;
    }

    if (offset == size) {
        parser->error = ERROR_UNEXPECTED_EOF;
        parser->off = offset - 1;
        return true;
    }
    ++offset;

    parser->data_size += data_size + 1;
    parser->off = offset;
    return false;
}


static bool
jml_json_key_size(jml_json_parser_t *parser)
{
    if (parser->flags & ALLOW_UNQUOTED_KEYS) {
        size_t offset = parser->off;
        const size_t size = parser->size;
        const char *src = parser->src;
        size_t data_size = parser->data_size;

        if (src[offset] == '"')
            return jml_json_string_size(parser);
        else if ((parser->flags & ALLOW_SINGLE_QUOTED) && (src[offset] == '\''))
            return jml_json_string_size(parser);
        else {
            while ((offset < size) && jml_is_identnum(src[offset])) {
                ++offset;
                ++data_size;
            }

            ++data_size;
            parser->off = offset;
            parser->data_size = data_size;
            return false;
        }
    } else
        return jml_json_string_size(parser);
}


static bool
jml_json_object_size(jml_json_parser_t *parser, bool global)
{
    const char *src = parser->src;
    const size_t size = parser->size;
    size_t elements = 0;
    bool allow_comma = false;
    bool found_closing_brace = false;

    if (global) {
        if (!jml_json_skip_all(parser) && src[parser->off] == '{')
            global = false;
    }

    if (!global) {
        if (src[parser->off] != '{') {
            parser->error = ERROR_UNKNOWN;
            return true;
        }
        ++parser->off;
    }

    if ((parser->off == size) && !global) {
        parser->error = ERROR_UNEXPECTED_EOF;
        return true;
    }

    do {
        if (!global) {
            if (jml_json_skip_all(parser)) {
                parser->error = ERROR_UNEXPECTED_EOF;
                return true;
            }

            if (src[parser->off] == '}') {
                ++parser->off;
                found_closing_brace = true;
                break;
            }
        } else if (jml_json_skip_all(parser))
            break;

        if (allow_comma) {
            if (src[parser->off] == ',') {
                ++parser->off;
                allow_comma = false;
            } else if (parser->flags & ALLOW_NO_COMMAS)
                allow_comma = false;
            else {
                parser->error = ERROR_EXPECTED_CLOSING;
                return true;
            }

            if (parser->flags & ALLOW_TRAILING_COMMA)
                continue;
            else if (jml_json_skip_all(parser)) {
                parser->error = ERROR_UNEXPECTED_EOF;
                return true;
            }
        }

        if (jml_json_key_size(parser)) {
            parser->error = ERROR_INVALID_STRING;
            return true;
        }

        if (jml_json_skip_all(parser)) {
            parser->error = ERROR_UNEXPECTED_EOF;
            return true;
        }

        if (parser->flags & ALLOW_EQUALS) {
            if ((src[parser->off] != ':') && (src[parser->off] != '=')) {
                parser->error = ERROR_EXPECTED_COLON;
                return true;
            }
        } else {
            if (src[parser->off] != ':') {
                parser->error = ERROR_EXPECTED_COLON;
                return true;
            }
        }
        ++parser->off;

        if (jml_json_skip_all(parser)) {
            parser->error = ERROR_UNEXPECTED_EOF;
            return true;
        }

        if (jml_json_value_size(parser, false))
            return true;

        ++elements;
        allow_comma = true;
    } while (parser->off < size);

    if ((parser->off == size) && !global && !found_closing_brace) {
        parser->error = ERROR_UNEXPECTED_EOF;
        return true;
    }

    return false;
}


static bool
jml_json_array_size(jml_json_parser_t *parser)
{
    size_t elements = 0;
    bool allow_comma = false;
    const char *src = parser->src;
    const size_t size = parser->size;

    if (src[parser->off] != '[') {
        parser->error = ERROR_UNKNOWN;
        return true;
    }
    ++parser->off;

    while (parser->off < size) {
        if (jml_json_skip_all(parser)) {
            parser->error = ERROR_UNEXPECTED_EOF;
            return true;
        }

        if (src[parser->off] == ']') {
            ++parser->off;
            return false;
        }

        if (allow_comma) {
            if (src[parser->off] == ',') {
                ++parser->off;
                allow_comma = false;
            } else if (!(parser->flags & ALLOW_NO_COMMAS)) {
                parser->error = ERROR_EXPECTED_CLOSING;
                return true;
            }

            if (parser->flags & ALLOW_TRAILING_COMMA) {
                allow_comma = false;
                continue;
            } else if (jml_json_skip_all(parser)) {
                parser->error = ERROR_UNEXPECTED_EOF;
                return true;
            }
        }

        if (jml_json_value_size(parser, false))
            return true;

        ++elements;
        allow_comma = true;
    }

    parser->error = ERROR_UNEXPECTED_EOF;
    return true;
}


static bool
jml_json_number_size(jml_json_parser_t *parser)
{
    size_t offset = parser->off;
    const size_t size = parser->size;
    bool had_leading_digits = false;
    const char *src = parser->src;

    if ((parser->flags & ALLOW_HEX_NUMBER) && (offset + 1 < size)
        && ('0' == src[offset])
        && (('x' == src[offset + 1]) || ('X' == src[offset + 1]))) {
        offset += 2;

        while ((offset < size) && jml_is_hex(src[offset]))
            ++offset;
    } else {
        bool found_sign = 0;
        bool inf_or_nan = 0;

        if ((offset < size) && (('-' == src[offset]) ||
            ((parser->flags && ALLOW_PLUS_SIGN) && ('+' == src[offset])))) {
            ++offset;
            found_sign = true;
        }

        if (parser->flags & ALLOW_INF_NAN) {
            const char inf[] = "Infinity";
            const char nan[] = "NaN";

            const size_t inf_strlen = sizeof(inf) - 1;
            const size_t nan_strlen = sizeof(nan) - 1;

            if (offset + inf_strlen < size) {
                bool found = true;
                for (size_t i = 0; i < inf_strlen; i++) {
                    if (inf[i] != src[offset + i]) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    offset += inf_strlen;
                    inf_or_nan = 1;
                }
            }

            if (offset + nan_strlen < size) {
                bool found = true;
                for (size_t i = 0; i < nan_strlen; i++) {
                    if (nan[i] != src[offset + i]) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    offset += nan_strlen;
                    inf_or_nan = 1;
                }
            }
        }

        if (found_sign && !inf_or_nan && (offset < size)
            && !jml_is_digit(src[offset])) {
            if (!(parser->flags & ALLOW_DECIMAL_POINT) || (src[offset] != '.')) {
                parser->error = ERROR_INVALID_NUMBER;
                parser->off = offset;
                return true;
            }
        }

        if ((offset < size) && (src[offset] == '0')) {
            ++offset;
            had_leading_digits = true;

            if ((offset < size) && jml_is_digit(src[offset])) {
                parser->error = ERROR_INVALID_NUMBER;
                parser->off = offset;
                return true;
            }
        }

        while ((offset < size) && jml_is_digit(src[offset])) {
            ++offset;
            had_leading_digits = true;
        }

        if ((offset < size) && (src[offset] == '.')) {
            ++offset;

            if (!jml_is_digit(src[offset])) {
                if (!(parser->flags & ALLOW_DECIMAL_POINT) || !had_leading_digits) {
                    parser->error = ERROR_INVALID_NUMBER;
                    parser->off = offset;
                    return true;
                }
            }

            while ((offset < size) && jml_is_digit(src[offset]))
                ++offset;
        }

        if ((offset < size) && ('e' == src[offset] || 'E' == src[offset])) {
            ++offset;

            if ((offset < size) && ('-' == src[offset] || '+' == src[offset]))
                ++offset;

            if ((offset < size) && !jml_is_digit(src[offset])) {
                parser->error = ERROR_INVALID_NUMBER;
                parser->off = offset;
                return true;
            }

            do {
                ++offset;
            } while ((offset < size) && jml_is_digit(src[offset]));
        }
    }

    if (offset < size) {
        switch (src[offset]) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '}':
            case ',':
            case ']':
                break;

            case '=':
                if (parser->flags & ALLOW_EQUALS)
                    break;
                /*fallthrough*/

            default:
                parser->error = ERROR_INVALID_NUMBER;
                parser->off = offset;
                return true;
        }
    }

    parser->data_size += offset - parser->off;
    ++parser->data_size;
    parser->off = offset;
    return false;
}


static bool
jml_json_value_size(jml_json_parser_t *parser, bool global)
{
    const char *src = parser->src;
    size_t offset;
    const size_t size = parser->size;

    if (global)
        return jml_json_object_size(parser, true);
    else {
        if (jml_json_skip_all(parser)) {
            parser->error = ERROR_UNEXPECTED_EOF;
            return true;
        }
        offset = parser->off;

        switch (src[offset]) {
            case '"':
                return jml_json_string_size(parser);

            case '\'':
                if (parser->flags & ALLOW_SINGLE_QUOTED)
                    return jml_json_string_size(parser);
                else {
                    parser->error = ERROR_INVALID_VALUE;
                    return true;
                }

            case '{':
                return jml_json_object_size(parser, false);

            case '[':
                return jml_json_array_size(parser);

            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return jml_json_number_size(parser);

            case '+':
                if (parser->flags & ALLOW_PLUS_SIGN)
                    return jml_json_number_size(parser);
                else {
                    parser->error = ERROR_INVALID_NUMBER;
                    return true;
                }

            case '.':
                if (parser->flags & ALLOW_DECIMAL_POINT)
                    return jml_json_number_size(parser);
                else {
                    parser->error = ERROR_INVALID_NUMBER;
                    return true;
                }

            default:
                if ((offset + 4) <= size && 't' == src[offset] &&
                    'r' == src[offset + 1] && 'u' == src[offset + 2] &&
                    'e' == src[offset + 3]) {

                    parser->off += 4;
                    return false;
                } else if ((offset + 5) <= size && 'f' == src[offset] &&
                    'a' == src[offset + 1] && 'l' == src[offset + 2] &&
                    's' == src[offset + 3] && 'e' == src[offset + 4]) {

                    parser->off += 5;
                    return false;
                } else if ((offset + 4) <= size && 'n' == parser->src[offset] &&
                    'u' == parser->src[offset + 1] &&
                    'l' == parser->src[offset + 2] &&
                    'l' == parser->src[offset + 3]) {

                    parser->off += 4;
                    return false;
                } else if ((parser->flags & ALLOW_INF_NAN) &&
                    (offset + 3) <= size && 'N' == src[offset] &&
                    'a' == src[offset + 1] && 'N' == src[offset + 2])

                    return jml_json_number_size(parser);
                else if ((parser->flags & ALLOW_INF_NAN) &&
                    (offset + 8) <= size && 'I' == src[offset] &&
                    'n' == src[offset + 1] && 'f' == src[offset + 2] &&
                    'i' == src[offset + 3] && 'n' == src[offset + 4] &&
                    'i' == src[offset + 5] && 't' == src[offset + 6] &&
                    'y' == src[offset + 7])

                    return jml_json_number_size(parser);

                parser->error = ERROR_INVALID_VALUE;
                return true;
        }
    }
}


static jml_value_t jml_json_value_parse(jml_json_parser_t *parser, bool global);


static jml_obj_string_t *
jml_json_string_parse(jml_json_parser_t *parser)
{
    size_t offset = parser->off;
    size_t bytes_written = 0;
    const char *const src = parser->src;
    const char quote_to_use = '\'' == src[offset] ? '\'' : '"';
    char *data = parser->data;
    uint64_t high_surrogate = 0;
    uint64_t codepoint;

    ++offset;
    while (quote_to_use != src[offset]) {
        if ('\\' == src[offset]) {
            ++offset;

            switch (src[offset++]) {
                case 'u': {
                    codepoint = 0;
                    if (!jml_json_hex_value(&src[offset], 4, &codepoint))
                        return NULL;
                    offset += 4;

                    if (codepoint <= 0x7fu)
                        data[bytes_written++] = (char)codepoint;
                    else if (codepoint <= 0x7ffu) {
                        data[bytes_written++] = (char)(0xc0u | (codepoint >> 6));
                        data[bytes_written++] = (char)(0x80u | (codepoint & 0x3fu));
                    } else if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        high_surrogate = codepoint;
                        continue;
                    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                        const uint64_t surrogate_offset = 0x10000u - (0xd800u << 10) - 0xdc00u;
                        codepoint = (high_surrogate << 10) + codepoint + surrogate_offset;
                        high_surrogate = 0;

                        data[bytes_written++] = (char)(0xf0u | (codepoint >> 18));
                        data[bytes_written++] = (char)(0x80u | ((codepoint >> 12) & 0x3fu));
                        data[bytes_written++] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
                        data[bytes_written++] = (char)(0x80u | (codepoint & 0x3fu));
                    } else {
                        data[bytes_written++] = (char)(0xe0u | (codepoint >> 12));
                        data[bytes_written++] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
                        data[bytes_written++] = (char)(0x80u | (codepoint & 0x3fu));
                    }
                    break;
                }

                case '"':
                    data[bytes_written++] = '"';
                    break;

                case '\\':
                    data[bytes_written++] = '\\';
                    break;

                case '/':
                    data[bytes_written++] = '/';
                    break;

                case 'b':
                    data[bytes_written++] = '\b';
                    break;

                case 'f':
                    data[bytes_written++] = '\f';
                    break;

                case 'n':
                    data[bytes_written++] = '\n';
                    break;

                case 'r':
                    data[bytes_written++] = '\r';
                    break;

                case 't':
                    data[bytes_written++] = '\t';
                    break;

                case '\r':
                    data[bytes_written++] = '\r';

                    if (src[offset] == '\n') {
                        data[bytes_written++] = '\n';
                        ++offset;
                    }
                    break;

                case '\n':
                    data[bytes_written++] = '\n';
                    break;

                default:
                    return NULL;
            }
        } else
            data[bytes_written++] = src[offset++];
    }

    parser->off = ++offset;
    data[bytes_written + 1] = '\0';
    return jml_obj_string_copy(data, bytes_written);
}


static jml_obj_string_t *
jml_json_key_parse(jml_json_parser_t *parser)
{
    if (parser->flags & ALLOW_UNQUOTED_KEYS) {
        const char *src = parser->src;
        char *data = parser->data;
        size_t offset = parser->off;

        if ((src[offset] == '"') || (src[offset] == '\''))
            return jml_json_string_parse(parser);
        else {
            size_t size = 0;
            while (jml_is_identnum(src[offset]))
                data[size++] = src[offset++];

            data[size] = '\0';
            parser->data += size;
            parser->off = offset;
            return jml_obj_string_copy(data, size);
        }
    } else
        return jml_json_string_parse(parser);
}


static jml_obj_map_t *
jml_json_object_parse(jml_json_parser_t *parser, bool global)
{
    const size_t size = parser->size;
    const char *const src = parser->src;
    size_t elements = 0;
    bool allow_comma = false;

    if (global) {
        if ('{' == src[parser->off])
            global = false;
    }

    if (!global)
        ++parser->off;

    (void)jml_json_skip_all(parser);

    jml_obj_map_t *map = jml_obj_map_new();
    jml_gc_exempt_push(OBJ_VAL(map));

    while (parser->off < size) {
        jml_obj_string_t *key = NULL;
        jml_value_t value = NONE_VAL;

        if (!global) {
            (void)jml_json_skip_all(parser);

            if ('}' == src[parser->off]) {
                ++parser->off;
                break;
            }
        } else if (jml_json_skip_all(parser))
            break;

        if (allow_comma && src[parser->off] == ',') {
            ++parser->off;
            allow_comma = false;
            continue;
        }

        key = jml_json_key_parse(parser);
        (void)jml_json_skip_all(parser);

        ++parser->off;
        (void)jml_json_skip_all(parser);

        value = jml_json_value_parse(parser, false);
        jml_gc_exempt_push(value);
        jml_hashmap_set(&map->hashmap, key, value);
        jml_gc_exempt_pop();

        ++elements;
        allow_comma = true;
    }

    jml_gc_exempt_pop();
    return map;
}


static jml_obj_array_t *
jml_json_array_parse(jml_json_parser_t *parser)
{
    const char *const src = parser->src;
    const size_t size = parser->size;
    size_t elements = 0;
    bool allow_comma = false;

    ++parser->off;
    (void)jml_json_skip_all(parser);

    jml_obj_array_t *array = jml_obj_array_new();
    jml_gc_exempt_push(OBJ_VAL(array));

    do {
        jml_value_t value = NONE_VAL;

        (void)jml_json_skip_all(parser);

        if (src[parser->off] == ']') {
            ++parser->off;
            break;
        }

        if (allow_comma && src[parser->off] == ',') {
            ++parser->off;
            allow_comma = false;
            continue;
        }

        value = jml_json_value_parse(parser, false);
        jml_gc_exempt_push(value);
        jml_obj_array_append(array, value);
        jml_gc_exempt_pop();

        ++elements;
        allow_comma = true;
    } while (parser->off < size);

    jml_gc_exempt_pop();
    return array;
}


static jml_value_t
jml_json_number_parse(jml_json_parser_t *parser)
{
    size_t offset = parser->off;
    const size_t size = parser->size;
    size_t bytes_written = 0;
    const char *src = parser->src;
    char *data = parser->data;

    if (parser->flags & ALLOW_HEX_NUMBER) {
        if (('0' == src[offset]) && (('x' == src[offset + 1])
            || ('X' == src[offset + 1]))) {

            while ((offset < size) && (jml_is_hex(src[offset]) ||
                ('x' == src[offset]) || ('X' == src[offset])))
                data[bytes_written++] = src[offset++];
        }
    }

    while (offset < size) {
        bool end = false;

        switch (src[offset]) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.':
            case 'e':
            case 'E':
            case '+':
            case '-':
                data[bytes_written++] = src[offset++];
                break;

            default:
                end = true;
                break;
        }

        if (end)
            break;
    }

    if (parser->flags & ALLOW_INF_NAN) {
        const size_t inf_strlen = 8;
        const size_t nan_strlen = 3;

        if (offset + inf_strlen < size) {
            if ('I' == src[offset]) {
                for (size_t i = 0; i < inf_strlen; i++)
                    data[bytes_written++] = src[offset++];
            }
        }

        if (offset + nan_strlen < size) {
            if ('N' == src[offset]) {
                for (size_t i = 0; i < nan_strlen; i++)
                    data[bytes_written++] = src[offset++];
            }
        }
    }

    data[bytes_written++] = '\0';
    parser->data += bytes_written;
    parser->off = offset;

    double num = strtod(data, NULL);
    return NUM_VAL(num);
}


static jml_value_t
jml_json_value_parse(jml_json_parser_t *parser, bool global)
{
    const char *src = parser->src;
    const size_t size = parser->size;

    (void)jml_json_skip_all(parser);
    size_t offset = parser->off;

    if (global)
        return OBJ_VAL(jml_json_object_parse(parser, true));
    else {
        switch (src[offset]) {
            case '"':
            case '\'':
                return OBJ_VAL(jml_json_string_parse(parser));

            case '{':
                return OBJ_VAL(jml_json_object_parse(parser, false));

            case '[':
                return OBJ_VAL(jml_json_array_parse(parser));

            case '-':
            case '+':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.':
                return jml_json_number_parse(parser);

            default:
                if ((offset + 4) <= size && 't' == src[offset] &&
                    'r' == src[offset + 1] && 'u' == src[offset + 2] &&
                    'e' == src[offset + 3]) {

                    parser->off += 4;
                    return TRUE_VAL;
                } else if ((offset + 5) <= size && 'f' == src[offset] &&
                    'a' == src[offset + 1] && 'l' == src[offset + 2] &&
                    's' == src[offset + 3] && 'e' == src[offset + 4]) {

                    parser->off += 5;
                    return FALSE_VAL;
                } else if ((offset + 4) <= size && 'n' == src[offset] &&
                    'u' == src[offset + 1] && 'l' == src[offset + 2] &&
                    'l' == src[offset + 3]) {

                    parser->off += 4;
                    return NONE_VAL;
                } else if ((parser->flags & ALLOW_INF_NAN) &&
                    (offset + 3) <= size && 'N' == src[offset] &&
                    'a' == src[offset + 1] && 'N' == src[offset + 2])

                     return jml_json_number_parse(parser);
                else if ((parser->flags & ALLOW_INF_NAN) &&
                    (offset + 8) <= size && 'I' == src[offset] &&
                    'n' == src[offset + 1] && 'f' == src[offset + 2] &&
                    'i' == src[offset + 3] && 'n' == src[offset + 4] &&
                    'i' == src[offset + 5] && 't' == src[offset + 6] &&
                    'y' == src[offset + 7])

                    return jml_json_number_parse(parser);
        }
    }
    return NONE_VAL;
}


static jml_json_error_t
jml_json_parse(const char *src, size_t size, jml_json_mode flags,
    jml_value_t *value)
{
    jml_json_parser_t parser;
    jml_json_error_t error = {0};

    if (src == NULL || value == NULL) {
        error.error = ERROR_UNKNOWN;
        return error;
    }

    parser.src = src;
    parser.size = size;
    parser.off = 0;
    parser.line_no = 1;
    parser.line_off = 0;
    parser.error = ERROR_NONE;
    parser.data = NULL;
    parser.data_size = 0;
    parser.flags = flags;

    bool input_error = jml_json_value_size(
        &parser, (bool)(ALLOW_GLOBAL & parser.flags)
    );

    if (input_error == false) {
        jml_json_skip_all(&parser);

        if (parser.off != parser.size) {
            parser.error = ERROR_UNEXPECTED_CHARS;
            input_error = true;
        }
    }

    if (input_error) {
        error.error = parser.error;
        error.off = parser.off;
        error.line_no = parser.line_no;
        error.col_no = parser.off - parser.line_off;
        return error;
    }

    /*buffer*/
    char *allocation = jml_realloc(NULL, parser.data_size);

    parser.off = 0;
    parser.line_no = 1;
    parser.line_off = 0;
    parser.data = allocation;

    *value = jml_json_value_parse(&parser, (bool)(ALLOW_GLOBAL & parser.flags));

    jml_realloc(allocation, 0);
    error.error = ERROR_NONE;
    return error;
}


static bool
jml_json_value_unparse(char *buffer, size_t *size,
    size_t *pos, jml_value_t value)
{
    if (IS_OBJ(value)) {
        switch (OBJ_TYPE(value)) {
            case OBJ_STRING: {
                jml_obj_string_t *string = AS_STRING(value);
                REALLOC(char, buffer, *size, *pos + string->length + 2);
                *pos += sprintf(buffer + *pos, "\"");

                memcpy(buffer + *pos, string->chars, string->length);
                *pos += string->length;

                *pos += sprintf(buffer + *pos, "\"");
                break;
            }

            case OBJ_ARRAY: {
                jml_obj_array_t *array = AS_ARRAY(value);
                REALLOC(char, buffer, *size, *pos + 2);
                *pos += sprintf(buffer + *pos, "[");

                for (int i = 0; i < (array->values.count - 1); ++i) {
                    jml_json_value_unparse(buffer, size, pos,
                        array->values.values[i]);

                    REALLOC(char, buffer, *size, *pos + 2);
                    *pos += sprintf(buffer + *pos, ", ");
                }

                jml_json_value_unparse(
                    buffer, size, pos,
                    array->values.values[array->values.count - 1]
                );

                REALLOC(char, buffer, *size, *pos + 2);
                *pos += sprintf(buffer + *pos, "]");
                break;
            }

            case OBJ_MAP: {
                jml_obj_map_t *map = AS_MAP(value);
                jml_hashmap_entry_t *entries = jml_hashmap_iterator(&map->hashmap);
                REALLOC(char, buffer, *size, *pos + 2);
                *pos += sprintf(buffer + *pos, "{");

                for (int i = 0; i < (map->hashmap.count - 1); ++i) {
                    jml_json_value_unparse(buffer, size, pos,
                        OBJ_VAL(entries[i].key));

                    REALLOC(char, buffer, *size, *pos + 2);
                    *pos += sprintf(buffer + *pos, ": ");

                    jml_json_value_unparse(buffer, size, pos,
                        entries[i].value);

                    REALLOC(char, buffer, *size, *pos + 2);
                    *pos += sprintf(buffer + *pos, ", ");
                }

                jml_json_value_unparse(buffer, size, pos,
                    OBJ_VAL(entries[map->hashmap.count - 1].key));

                REALLOC(char, buffer, *size, *pos + 2);
                *pos += sprintf(buffer + *pos, ": ");

                jml_json_value_unparse(buffer, size, pos,
                    entries[map->hashmap.count - 1].value);

                jml_realloc(entries, 0);
                REALLOC(char, buffer, *size, *pos + 2);
                *pos += sprintf(buffer + *pos, "}");
                break;
            }

            default:
                return false;
        }
    } else if (IS_NONE(value)) {
        REALLOC(char, buffer, *size, *pos + 5);
        *pos += sprintf(buffer + *pos, "null");

    } else if (IS_BOOL(value)) {
        REALLOC(char, buffer, *size, *pos + 5);
        *pos += sprintf(buffer + *pos, BOOL_VAL(value) ? "true" : "false");

    } else if (IS_NUM(value)) {
        char numbuf[64];
        int numlen = snprintf(numbuf, 64, "%g", AS_NUM(value));
        REALLOC(char, buffer, *size, *pos + numlen);
        *pos += sprintf(buffer + *pos, "%.*s", numlen, numbuf);
    }

    return true;
}


static jml_value_t
jml_std_json_parse(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    jml_obj_string_t *string = AS_STRING(args[0]);
    jml_value_t value;

    jml_json_error_t error = jml_json_parse(
        string->chars, string->length, JSON_LENIENT, &value
    );

    if (error.error != ERROR_NONE) {
        const char *errors[] = {
            "Expected closing bracket or comma",
            "Expected colon",
            "Expected quote",
            "Invalid string escape sequence",
            "Invalid number",
            "Invalid value",
            "Invalid string",
            "Unexpected eof",
            "Unexpected trailing characters",
            "Unknown error"
        };

        exc = jml_obj_exception_format(
            "JsonErr", "%s on line %u column %u (char %u).",
            errors[error.error - 1],
            error.line_no, error.col_no, error.off
        );
        goto err;
    }

    return value;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_json_unparse(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    if (!IS_MAP(args[0])) {
        exc = jml_error_types(false, 1, "map");
        return OBJ_VAL(exc);
    }

    size_t size = 64;
    size_t pos = 0;
    char *buffer = jml_realloc(NULL, size);

    if (!jml_json_value_unparse(buffer, &size, &pos, args[0])) {
        jml_realloc(buffer, 0);
        exc = jml_obj_exception_new(
            "JsonErr", "Invalid value to unparse"
        );
        return OBJ_VAL(exc);
    }

    return OBJ_VAL(jml_obj_string_take(buffer, size));
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"parse",                       &jml_std_json_parse},
    {"unparse",                     &jml_std_json_unparse},
    {NULL,                          NULL}
};
