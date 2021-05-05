//--link: hs

#include <hs/hs.h>

#include <jml.h>

#include <stdio.h>


static hs_scratch_t *scratch        = NULL;


typedef struct {
    const char                     *source;
    jml_obj_array_t                *array;
    bool                            matched;
} jml_hs_context_t;


static int
jml_hs_callback(unsigned int id, unsigned long long from,
    unsigned long long to, unsigned int flags, void *ctx)
{
    (void)id;
    (void)flags;

    jml_hs_context_t *context = (jml_hs_context_t*)ctx;
    context->matched = true;

    if (context->array != NULL) {
        jml_obj_string_t *string = jml_obj_string_copy(
            context->source + from, to);
        jml_obj_array_append(context->array, OBJ_VAL(string));
        return HS_SUCCESS;
    }

    return HS_SCAN_TERMINATED;
}


static jml_obj_exception_t *
jml_hs_match_array(const char *pattern,
    jml_obj_string_t *source, jml_obj_array_t *array, bool *matched)
{
    jml_obj_exception_t *exc = NULL;

    hs_database_t *database;
    hs_compile_error_t *compile_err;
    hs_error_t error = hs_compile(
        pattern, HS_FLAG_DOTALL | HS_FLAG_MULTILINE, HS_MODE_BLOCK,
        NULL, &database, &compile_err
    );

    if (error != HS_SUCCESS) {
        exc = jml_obj_exception_format(
            "hsErr", "%s", compile_err->message
        );
        hs_free_compile_error(compile_err);
        return exc;
    }

    if (hs_alloc_scratch(database, &scratch) != HS_SUCCESS) {
        exc = jml_obj_exception_new(
            "hsErr", "Unable to allocate scratch."
        );
        hs_free_database(database);
        return exc;
    }

    jml_hs_context_t context;
    context.array = array;
    context.matched = false;
    context.source = source->chars;

    error = hs_scan(
        database, source->chars, source->length, 0,
        scratch, jml_hs_callback, &context
    );

    if (error != HS_SUCCESS && error != HS_SCAN_TERMINATED) {
        exc = jml_obj_exception_new(
            "hsErr", "Unable to scan input buffer."
        );
        hs_free_database(database);
        return exc;
    }

    if (matched != NULL)
        *matched = context.matched;

    hs_free_database(database);
    return NULL;
}


static jml_value_t
jml_std_hs_contain(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    const char *pattern = AS_CSTRING(args[0]);
    jml_obj_string_t *source = AS_STRING(args[1]);

    bool matched = false;

    exc = jml_hs_match_array(pattern, source, NULL, &matched);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return BOOL_VAL(matched);
}


static jml_value_t
jml_std_hs_search(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    const char *pattern = AS_CSTRING(args[0]);
    jml_obj_string_t *source = AS_STRING(args[1]);

    jml_obj_array_t *array = jml_obj_array_new();
    jml_gc_exempt_push(OBJ_VAL(array));

    exc = jml_hs_match_array(pattern, source, array, NULL);

    if (exc != NULL) {
        jml_gc_exempt_pop();
        return OBJ_VAL(exc);
    }

    return jml_gc_exempt_pop();
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"contain",                     &jml_std_hs_contain},
    {"search",                      &jml_std_hs_search},
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_free(JML_UNUSED(jml_obj_module_t *module))
{
    hs_free_scratch(scratch);
}
