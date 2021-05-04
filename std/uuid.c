//--link: uuid

#include <jml.h>


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#include <uuid/uuid.h>

#else

#error "Current platform not supported."

#endif


static jml_value_t
jml_std_uuid_uuid(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    uuid_t uuid;
    uuid_generate_random(uuid);

    char *buffer = jml_realloc(NULL, UUID_STR_LEN);
    uuid_unparse(uuid, buffer);

    return OBJ_VAL(
        jml_obj_string_take(buffer, UUID_STR_LEN)
    );
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"uuid",                        &jml_std_uuid_uuid},
    {NULL,                          NULL}
};
