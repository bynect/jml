//--link: pcre

#include <jml.h>

#include <pcre.h>


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
jml_std_rex_pcre_patter_find(int arg_count, jml_value_t *args)
{
    (void)arg_count;
    (void)args;

    //TODO

    return NONE_VAL;
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
