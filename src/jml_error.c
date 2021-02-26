#include <stdarg.h>
#include <stdio.h>

#include <jml.h>

#include <jml_gc.h>


jml_obj_exception_t *
jml_error_args(int arg_count, int expected_arg)
{
    char message[128];

    sprintf(message, "Expected '%d' arguments but got '%d'.",
        expected_arg, arg_count);

    if (arg_count > expected_arg)
        return jml_obj_exception_new(
            "TooManyArgs", message);

    if (arg_count < expected_arg)
        return jml_obj_exception_new(
            "TooFewArgs", message);

    return NULL;
}


jml_obj_exception_t *
jml_error_implemented(jml_value_t value)
{
    return jml_obj_exception_format(
        "NotImplemented",
        "Not implemented for %s.",
        jml_value_stringify_type(value)
    );
}


jml_obj_exception_t *
jml_error_types(bool mult, int arg_count, ...)
{
    va_list types;
    va_start(types, arg_count);

    size_t size                 = (arg_count + 1) * 32;
    size_t dest_size            = size;
    char  *message              = jml_realloc(NULL, size);
    char  *head                 = message;

    char *next = va_arg(types, char*);
    sprintf(message, "Expected arguments of <type %s>", next);

    for (int i = 1; i < arg_count; ++i) {
        char *next = va_arg(types, char*);
        char temp[128];

        sprintf(temp, mult ? " or <type %s>" : " and <type %s>", next);

        dest_size += strlen(temp);
        REALLOC(char, message, size, dest_size);

        head = jml_strcat(head, temp);
    }

    *head++ = '.';
    *head   = '\0';

    jml_obj_exception_t *exc = jml_obj_exception_new(
        "DiffTypes", message
    );

    jml_realloc(message, 0);
    va_end(types);

    return exc;
}


jml_obj_exception_t *
jml_error_value(const char *value)
{
    return jml_obj_exception_format(
        "WrongValue", "Invalid '%s'.", value
    );
}
