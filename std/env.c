#ifdef __GNUC__

#define _POSIX_C_SOURCE             200112l

#endif

#include <stdlib.h>

#include <jml.h>


static jml_value_t
jml_std_env_getenv(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    char *string = getenv(AS_CSTRING(args[0]));

    if (string != NULL)
        return jml_string_intern(string);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_env_setenv(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        exc = jml_error_types(false, 2, "string", "string");
        goto err;
    }

    if (setenv(AS_CSTRING(args[0]), AS_CSTRING(args[2]), true) == -1) {
        exc = jml_obj_exception_new(
            "EnvErr", "Setting environment variable failed."
        );
        goto err;
    }

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_env_unsetenv(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    if (unsetenv(AS_CSTRING(args[0])) == -1) {
        exc = jml_obj_exception_new(
            "EnvErr", "Unsetting environment variable failed."
        );
        goto err;
    }

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"getenv",                      &jml_std_env_getenv},
    {"setenv",                      &jml_std_env_setenv},
    {"unsetenv",                    &jml_std_env_unsetenv},
    {NULL,                          NULL}
};
