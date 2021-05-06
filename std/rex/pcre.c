//--link: pcre

#include <jml.h>

#include <pcre.h>
#include <stdio.h>

static jml_obj_class_t  *pattern_class  = NULL;
static jml_obj_string_t *pattern_string = NULL;
static jml_obj_string_t *flags_string   = NULL;


static jml_value_t
jml_std_rex_pcre_patter_init(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_NUM(args[1])) {
        exc = jml_error_types(false, 2, "string", "number");
        goto err;
    }

    jml_obj_instance_t *self    = AS_INSTANCE(args[2]);
    const char         *pattern = AS_CSTRING(args[0]);
    int                 option  = AS_NUM(args[1]);

    const char *error;
    int error_off = 0;

    self->extra = pcre_compile(
        pattern, option, &error, &error_off, NULL
    );

    if (self->extra == NULL) {
        exc = jml_obj_exception_format(
            "RexErr", "At offset %d, %s.", error_off, error
        );
        goto err;
    }

    jml_hashmap_set(&self->fields, pattern_string, args[0]);
    jml_hashmap_set(&self->fields, flags_string, args[1]);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_rex_pcre_patter_at(int arg_count, jml_value_t *args)
{
    const int ovecsize = 30;
    int ovector[30] = {0};

    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_NUM(args[1])) {
        exc = jml_error_types(false, 2, "string", "number");
        goto err;
    }

    jml_obj_instance_t *self    = AS_INSTANCE(args[2]);
    jml_obj_string_t   *subject = AS_STRING(args[0]);
    unsigned int        offset  = AS_NUM(args[1]);

    jml_value_t *flags_value;
    jml_hashmap_get(&self->fields, flags_string, &flags_value);

    if (self->extra == NULL || !IS_NUM(*flags_value)) {
        exc = jml_error_value("Pattern instance");
        goto err;
    }

    int match = pcre_exec(
        self->extra, NULL, subject->chars, (int)subject->length,
        offset, AS_NUM(*flags_value), ovector, ovecsize
    );

    if (match == PCRE_ERROR_NOMATCH)
        return NONE_VAL;
    else if (match < 0) {
        exc = jml_obj_exception_new(
            "RexErr", "Failed to match pattern."
        );
        goto err;
    }

    //FIXME
    if (match == 0)
        match = ovecsize / 3;

    if (match == 1) {
        char *sub_start = subject->chars + ovector[0];
        int sub_length = ovector[1] - ovector[0];

        char *buffer = jml_realloc(NULL, sub_length + 1);
        memcpy(buffer, sub_start, sub_length);
        buffer[sub_length] = '\0';

        return OBJ_VAL(jml_obj_string_take(buffer, sub_length));
    }

    jml_obj_array_t *array = jml_obj_array_new();
    jml_gc_exempt_push(OBJ_VAL(array));

    for (int i = 0; i < match; ++i) {
        char *sub_start = subject->chars + ovector[2 * i];
        int sub_length = ovector[2 * i + 1] - ovector[2 * i];

        char *buffer = jml_realloc(NULL, sub_length + 1);
        memcpy(buffer, sub_start, sub_length);
        buffer[sub_length] = '\0';

        jml_obj_array_append(
            array,
            OBJ_VAL(jml_obj_string_take(buffer, sub_length))
        );
    }

    return jml_gc_exempt_pop();

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_rex_pcre_patter_find(int arg_count, jml_value_t *args)
{
    const int ovecsize = 30;
    int ovector[30] = {0};

    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    jml_obj_instance_t *self    = AS_INSTANCE(args[1]);
    jml_obj_string_t   *subject = AS_STRING(args[0]);

    jml_value_t *flags_value;
    jml_hashmap_get(&self->fields, flags_string, &flags_value);

    if (self->extra == NULL || !IS_NUM(*flags_value)) {
        exc = jml_error_value("Pattern instance");
        goto err;
    }

    int flags = AS_NUM(*flags_value);
    unsigned int option_bits;
    pcre_fullinfo(self->extra, NULL, PCRE_INFO_OPTIONS, &option_bits);

    bool utf8 = option_bits & PCRE_UTF8;
    option_bits &= PCRE_NEWLINE_CR | PCRE_NEWLINE_LF | PCRE_NEWLINE_CRLF
        | PCRE_NEWLINE_ANY|PCRE_NEWLINE_ANYCRLF;

    if (option_bits == 0) {
        int d;
        pcre_config(PCRE_CONFIG_NEWLINE, &d);

        option_bits = (d == 13) ? PCRE_NEWLINE_CR :
            (d == 10) ? PCRE_NEWLINE_LF :
            (d == (13 << 8 | 10)) ? PCRE_NEWLINE_CRLF :
            (d == -2) ? PCRE_NEWLINE_ANYCRLF :
            (d == -1) ? PCRE_NEWLINE_ANY : 0;
    }

    bool crlf_terminator = option_bits == PCRE_NEWLINE_ANY
        || option_bits == PCRE_NEWLINE_CRLF
        || option_bits == PCRE_NEWLINE_ANYCRLF;

    jml_obj_array_t *array = jml_obj_array_new();
    jml_gc_exempt_push(OBJ_VAL(array));

    int match;
    while (true) {
        int options = flags;
        int start_offset = ovector[1];

        if (ovector[0] == ovector[1]) {
            if (ovector[0] == (int)subject->length)
                break;

            options = PCRE_NOTEMPTY_ATSTART | PCRE_ANCHORED;
        }

        match = pcre_exec(
            self->extra, NULL, subject->chars, (int)subject->length,
            start_offset, options, ovector, ovecsize
        );

        if (match == PCRE_ERROR_NOMATCH) {
            if (options == 0)
                break;

            ovector[1] = start_offset + 1;

            if (crlf_terminator && start_offset < (int)subject->length - 1
                && subject->chars[start_offset] == '\r'
                && subject->chars[start_offset + 1] == '\n')
                ++ovector[1];
            else if (utf8) {
                while (ovector[1] < (int)subject->length) {
                    if ((subject->chars[ovector[1]] & 0xc0) != 0x80) break;
                    ++ovector[1];
                }
            }
            continue;
        } else if (match < 0) {
            exc = jml_obj_exception_new(
                "RexErr", "Failed to match pattern."
            );
            goto err;
        }

        //FIXME
        if (match == 0)
            match = ovecsize / 3;

        for (int i = 0; i < match; ++i) {
            char *sub_start = subject->chars + ovector[2 * i];
            int sub_length = ovector[2 * i + 1] - ovector[2 * i];

            char *buffer = jml_realloc(NULL, sub_length + 1);
            memcpy(buffer, sub_start, sub_length);
            buffer[sub_length] = '\0';

            jml_obj_array_append(
                array,
                OBJ_VAL(jml_obj_string_take(buffer, sub_length))
            );
        }
    }

    return jml_gc_exempt_pop();

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_rex_pcre_patter_free(int arg_count, jml_value_t *args)
{
    jml_obj_instance_t *self = AS_INSTANCE(args[arg_count - 1]);

    if (self->extra != NULL) {
        pcre_free(self->extra);
        self->extra = NULL;
    }

    return NONE_VAL;
}


/*class table*/
MODULE_TABLE_HEAD pattern_table[] = {
    {"__init",                      &jml_std_rex_pcre_patter_init},
    {"at",                          &jml_std_rex_pcre_patter_at},
    {"find",                        &jml_std_rex_pcre_patter_find},
    {"__free",                      &jml_std_rex_pcre_patter_free},
    {NULL,                          NULL}
};


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
    jml_module_add_value(module, "ANCHORED", NUM_VAL(PCRE_ANCHORED));
    jml_module_add_value(module, "AUTO_CALLOUT", NUM_VAL(PCRE_AUTO_CALLOUT));
    jml_module_add_value(module, "BSR_ANYCRLF", NUM_VAL(PCRE_BSR_ANYCRLF));
    jml_module_add_value(module, "BSR_UNICODE", NUM_VAL(PCRE_BSR_UNICODE));
    jml_module_add_value(module, "CASELESS", NUM_VAL(PCRE_CASELESS));
    jml_module_add_value(module, "DOLLAR_ENDONLY", NUM_VAL(PCRE_DOLLAR_ENDONLY));
    jml_module_add_value(module, "DOTALL", NUM_VAL(PCRE_DOTALL));
    jml_module_add_value(module, "DUPNAMES", NUM_VAL(PCRE_DUPNAMES));
    jml_module_add_value(module, "EXTENDED", NUM_VAL(PCRE_EXTENDED));
    jml_module_add_value(module, "EXTRA", NUM_VAL(PCRE_EXTRA));
    jml_module_add_value(module, "FIRSTLINE", NUM_VAL(PCRE_FIRSTLINE));
    jml_module_add_value(module, "JAVASCRIPT_COMPAT", NUM_VAL(PCRE_JAVASCRIPT_COMPAT));
    jml_module_add_value(module, "MULTILINE", NUM_VAL(PCRE_MULTILINE));
    jml_module_add_value(module, "NEWLINE_ANY", NUM_VAL(PCRE_NEWLINE_ANY));
    jml_module_add_value(module, "NEWLINE_ANYCRLF", NUM_VAL(PCRE_NEWLINE_ANYCRLF));
    jml_module_add_value(module, "NEWLINE_CR", NUM_VAL(PCRE_NEWLINE_CR));
    jml_module_add_value(module, "NEWLINE_CRLF", NUM_VAL(PCRE_NEWLINE_CRLF));
    jml_module_add_value(module, "NEWLINE_LF", NUM_VAL(PCRE_NEWLINE_LF));
    jml_module_add_value(module, "NO_AUTO_CAPTURE", NUM_VAL(PCRE_NO_AUTO_CAPTURE));
    jml_module_add_value(module, "UNGREEDY", NUM_VAL(PCRE_UNGREEDY));

#ifdef PCRE_UTF8

    jml_module_add_value(module, "UTF8", NUM_VAL(PCRE_UTF8));
    jml_module_add_value(module, "NO_UTF8_CHECK", NUM_VAL(PCRE_NO_UTF8_CHECK));

#else

    jml_module_add_value(module, "UTF8", NONE_VAL);
    jml_module_add_value(module, "NO_UTF8_CHECK", NONE_VAL);

#endif

    jml_module_add_class(module, "Pattern", pattern_table, false);

    //FIXME
    pattern_string = jml_obj_string_copy("pattern", 7);
    flags_string = jml_obj_string_copy("flags", 6);

    jml_value_t *pattern_value;
    if (jml_hashmap_get(&module->globals,
        jml_obj_string_copy("Pattern", 7), &pattern_value)) {

        pattern_class = AS_CLASS(*pattern_value);
        /*jml_gc_exempt(*pattern_value);*/
    }
}
