#if defined __GNUC__

#define _POSIX_C_SOURCE             200112l

#elif defined JML_PLATFORM_WIN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

#ifdef JML_PLATFORM_WIN
    char *var = AS_CSTRING(args[0]);
    size_t size = 0;

    getenv_s(&size, NULL, 0, var);
    if (size == 0)
        return NONE_VAL;

    char *string = jml_realloc(NULL, size * sizeof(char));
    getenv_s(&size, string, size, var);

#else
    char *string = getenv(AS_CSTRING(args[0]));
    if (string == NULL)
        return NONE_VAL;

#endif

    return jml_string_intern(string);

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

#ifdef JML_PLATFORM_WIN
    _putenv_s(AS_CSTRING(args[0]), AS_CSTRING(args[2]));

#else
    if (setenv(AS_CSTRING(args[0]), AS_CSTRING(args[2]), true) == -1) {
        exc = jml_obj_exception_new(
            "EnvErr", "Setting environment variable failed."
        );
        goto err;
    }
#endif

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

#if defined __GNUC__

    if (unsetenv(AS_CSTRING(args[0])) == -1) {
        exc = jml_obj_exception_new(
            "EnvErr", "Unsetting environment variable failed."
        );
        goto err;
    }

    return NONE_VAL;

#else

    exc = jml_obj_exception_format(
        "NotImplemented",
        "Function not supported on %s.",
        JML_PLATFORM_STRING
    );
    goto err;

#endif

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
