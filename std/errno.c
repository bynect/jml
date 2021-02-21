#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <jml.h>


static jml_value_t
jml_std_errno_get_errno(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL(errno);
}


static jml_value_t
jml_std_errno_strerror(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0])) {
        jml_core_exception_types(false, 1, "number");
        goto err;
    }

    char *str = strerror((int)AS_NUM(args[0]));
    return jml_string_intern(str);

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_errno_perror(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_core_exception_types(false, 1, "string");
        goto err;
    }

    perror(AS_CSTRING(args[0]));
    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"get_errno",                   &jml_std_errno_get_errno},
    {"strerror",                    &jml_std_errno_strerror},
    {"perror",                      &jml_std_errno_perror},
    {NULL,                          NULL}
};
