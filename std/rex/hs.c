//--link: hs

#include <hs/hs.h>

#include <jml.h>


static jml_obj_class_t  *database_class = NULL;
static jml_obj_string_t *pattern_string = NULL;
static jml_obj_string_t *flags_string   = NULL;

static hs_scratch_t     *scratch        = NULL;


typedef struct {
    const char                     *source;
    jml_obj_array_t                *array;
} jml_hs_context_t;


static int
jml_hs_callback(unsigned int id, unsigned long long from,
    unsigned long long to, unsigned int flags, void *ctx)
{
    (void)id;
    (void)flags;

    jml_hs_context_t *context = (jml_hs_context_t*)ctx;

    if (context->array != NULL) {
        jml_obj_string_t *string = jml_obj_string_copy(
            context->source + from, to);
        jml_obj_array_append(context->array, OBJ_VAL(string));
        return HS_SUCCESS;
    }

    return HS_SCAN_TERMINATED;
}


static jml_value_t
jml_std_rex_hs_database_init(int arg_count, jml_value_t *args)
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
    unsigned int        flags   = AS_NUM(args[1]);

    hs_database_t *database;
    hs_compile_error_t *compile_err;

    hs_error_t error = hs_compile(
        pattern, flags, HS_MODE_BLOCK,
        NULL, &database, &compile_err
    );

    if (error != HS_SUCCESS) {
        exc = jml_obj_exception_format(
            "RexErr", "%s", compile_err->message
        );
        hs_free_compile_error(compile_err);
        goto err;
    }

    if (hs_alloc_scratch(database, &scratch) != HS_SUCCESS) {
        exc = jml_obj_exception_new(
            "RexErr", "Unable to allocate scratch."
        );
        hs_free_database(database);
        goto err;
    }

    self->extra = database;

    jml_hashmap_set(&self->fields, pattern_string, args[0]);
    jml_hashmap_set(&self->fields, flags_string, args[1]);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_rex_hs_database_scan(int arg_count, jml_value_t *args)
{
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

    unsigned int        flags   = AS_NUM(*flags_value);
    jml_hs_context_t context;

    jml_obj_array_t *array = jml_obj_array_new();
    jml_gc_exempt_push(OBJ_VAL(array));
    context.array = array;
    context.source = subject->chars;

    hs_error_t error = hs_scan(
        self->extra, subject->chars, subject->length, flags,
        scratch, jml_hs_callback, &context
    );

    if (error != HS_SUCCESS && error != HS_SCAN_TERMINATED) {
        exc = jml_obj_exception_new(
            "RexErr", "Unable to scan input buffer."
        );
        jml_gc_exempt_pop();
        goto err;
    }

    return jml_gc_exempt_pop();

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_rex_hs_database_free(int arg_count, jml_value_t *args)
{
    jml_obj_instance_t *self = AS_INSTANCE(args[arg_count - 1]);

    if (self->extra != NULL) {
        hs_free_database(self->extra);
        self->extra = NULL;
    }

    return NONE_VAL;
}


/*class table*/
MODULE_TABLE_HEAD database_table[] = {
    {"__init",                      &jml_std_rex_hs_database_init},
    {"scan",                        &jml_std_rex_hs_database_scan},
    {"__free",                      &jml_std_rex_hs_database_free},
    {NULL,                          NULL}
};


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
    jml_module_add_value(module, "CASELESS", NUM_VAL(HS_FLAG_CASELESS));
    jml_module_add_value(module, "DOTALL", NUM_VAL(HS_FLAG_DOTALL));
    jml_module_add_value(module, "MULTILINE", NUM_VAL(HS_FLAG_MULTILINE));
    jml_module_add_value(module, "SINGLEMATCH", NUM_VAL(HS_FLAG_SINGLEMATCH));
    jml_module_add_value(module, "SINGLEMATCH", NUM_VAL(HS_FLAG_SINGLEMATCH));
    jml_module_add_value(module, "SINGLEMATCH", NUM_VAL(HS_FLAG_SINGLEMATCH));
    jml_module_add_value(module, "SOM_LEFTMOST", NUM_VAL(HS_FLAG_SOM_LEFTMOST));
    jml_module_add_value(module, "ALLOWEMPTY", NUM_VAL(HS_FLAG_ALLOWEMPTY));
    jml_module_add_value(module, "UTF8", NUM_VAL(HS_FLAG_UTF8));
    jml_module_add_value(module, "UCP", NUM_VAL(HS_FLAG_UCP));
    jml_module_add_value(module, "UTF8", NUM_VAL(HS_FLAG_UTF8));
    jml_module_add_value(module, "PREFILTER", NUM_VAL(HS_FLAG_PREFILTER));
    jml_module_add_value(module, "SOM_LEFTMOST", NUM_VAL(HS_FLAG_SOM_LEFTMOST));
    jml_module_add_value(module, "SOM_LEFTMOST", NUM_VAL(HS_FLAG_SOM_LEFTMOST));
    jml_module_add_value(module, "COMBINATION", NUM_VAL(HS_FLAG_COMBINATION));
    jml_module_add_value(module, "QUIET", NUM_VAL(HS_FLAG_QUIET));

    jml_module_add_class(module, "Database", database_table, false);

    //FIXME
    pattern_string = jml_obj_string_copy("pattern", 7);
    flags_string = jml_obj_string_copy("flags", 6);

    jml_value_t *database_value;
    if (jml_hashmap_get(&module->globals,
        jml_obj_string_copy("Database", 8), &database_value)) {

        database_class = AS_CLASS(*database_value);
        /*jml_gc_exempt(*database_value);*/
    }
}


MODULE_FUNC_HEAD
module_free(JML_UNUSED(jml_obj_module_t *module))
{
    if (scratch != NULL)
        hs_free_scratch(scratch);
}
