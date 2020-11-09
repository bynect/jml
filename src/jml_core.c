#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <jml.h>

#include <jml_module.h>
#include <jml_vm.h>
#include <jml_value.h>
#include <jml_util.h>


static jml_obj_exception_t *
jml_core_exception_args(int arg_count, int expected_arg)
{
    char message[30];

    sprintf(message, "Expected '%d', got '%d' args.", 
        expected_arg, arg_count);

    if (arg_count > expected_arg)
        return jml_obj_exception_new(
            "TooManyArgs", message);

    if (arg_count < expected_arg)
        return jml_obj_exception_new(
            "TooFewArgs", message);

    return NULL;
}


static jml_value_t
jml_core_clock(int arg_count, jml_value_t *args)
{

    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}


static jml_value_t
jml_core_print(int arg_count, jml_value_t *args)
{
    for (int i = 0; i < arg_count; i++) {
        jml_value_print(args[i]);
        printf("\n");
    }
    return NONE_VAL;
}


static jml_value_t
jml_core_print_fmt(int arg_count, jml_value_t *args)
{
    jml_value_t fmt_value = args[0];
    if (!IS_STRING(fmt_value)) {
        jml_obj_exception_t *exc = jml_obj_exception_new(
            "DiffTypes", "Expected argument of type 'string'."
        );

        return OBJ_VAL(exc);
    }

    jml_obj_string_t *fmt_obj = AS_STRING(fmt_value);
    char *fmt_str = jml_strdup(fmt_obj->chars);

    int fmt_args = 0;
    char dest[1024] = "";
    char *ptr = dest;
    char *token = jml_strtok(fmt_str, "{}");

    while (token != NULL) {
        ptr = strcat(ptr, token);
        ptr = strcat(ptr, jml_value_stringify(args[fmt_args + 1]));

        token = jml_strtok(NULL, "{}");
        ++fmt_args;
    }

    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, fmt_args);

    if (exc != NULL)
        return OBJ_VAL(exc);
    else
        fprintf(stdout, "%.*s\n", (int)strlen(dest) - 1, dest);

    return NONE_VAL;
}


typedef struct {
    const char                     *name;
    jml_cfunction                   function;
} jml_core_function;


jml_core_function core_functions[] = {
    {"clock",                       &jml_core_clock},
    {"print",                       &jml_core_print},
    {"printfmt",                    &jml_core_print_fmt},
};


void
jml_core_register(void)
{
    int length = (sizeof(core_functions) / sizeof(core_functions[0]));

    for (int i = 0; i < length; ++i) {
        jml_core_function current = core_functions[i];
        jml_cfunction_register(current.name, current.function);
    }
}
