#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <jml.h>

#include <jml_module.h>
#include <jml_vm.h>
#include <jml_value.h>
#include <jml_util.h>
#include <jml_gc.h>


/*core helper functions*/
static jml_obj_exception_t *
jml_core_exception_args(int arg_count, int expected_arg)
{
    char message[38];

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


/*core functions*/
static jml_value_t
jml_core_clock(int arg_count, jml_value_t *args)
{

    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL((double)clock() / CLOCKS_PER_SEC);
}


static jml_value_t
jml_core_print(int arg_count, jml_value_t *args)
{
    for (int i = 0; i < arg_count; ++i) {
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

    jml_obj_string_t *fmt_obj   = AS_STRING(fmt_value);
    char             *fmt_str   = jml_strdup(fmt_obj->chars);
    int               fmt_args  = 0;

    char dest[1024] = "";
    char *ptr = dest;

    char *token = jml_strtok(fmt_str, "{}");
    while (token != NULL) {
        ptr = strcat(ptr, token);

        char *value_str = jml_value_stringify(args[fmt_args + 1]);
        ptr = strcat(ptr, value_str);

        jml_realloc(value_str, 0UL);

        token = jml_strtok(NULL, "{}");
        ++fmt_args;
    }

    jml_realloc(token, 0UL);
    jml_realloc(fmt_str, 0UL);

    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, fmt_args);

    if (exc != NULL)
        return OBJ_VAL(exc);
    else
        fprintf(stdout, "%.*s\n", (int)strlen(dest) - 1, dest);

    return NONE_VAL;
}


static jml_value_t
jml_core_reverse(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 1
    );

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value = args[0];

    if (IS_STRING(value)) {
        jml_obj_string_t *string_obj = AS_STRING(value);

        char *str       = jml_strdup(string_obj->chars);
        int length      = string_obj->length;

        for (int i = 0; i < length / 2; ++i) {
            char temp = str[i];
            str[i] = str[length-1-i];
            str[length-1-i] = temp;
        }

        jml_obj_string_t *string_res = jml_obj_string_take(
            str, strlen(str));

        return OBJ_VAL(string_res);
    }

    return NONE_VAL;
}


static jml_value_t
jml_core_size(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 1
    );

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value = args[0];

    if (IS_STRING(value))
        return NUM_VAL(AS_STRING(value)->length);

    return NONE_VAL;
}


static jml_value_t
jml_core_instance(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 2
    );

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t instance                = args[0];
    jml_value_t klass                   = args[1];

    if (!IS_INSTANCE(instance) || !IS_CLASS(klass)) {

        jml_obj_exception_t *exc = jml_obj_exception_new(
            "DiffTypes", "Expected arguments of type 'instance' and 'class'."
        );
        return OBJ_VAL(exc);
    }

    jml_obj_instance_t *instance_obj    = AS_INSTANCE(instance);
    jml_obj_class_t    *klass_obj       = AS_CLASS(klass);

    return BOOL_VAL(instance_obj->klass == klass_obj);
}


static jml_value_t
jml_core_subclass(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 2
    );

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t sub                     = args[0];
    jml_value_t super                   = args[1];

    if (!IS_CLASS(sub) || !IS_CLASS(super)) {

        jml_obj_exception_t *exc = jml_obj_exception_new(
            "DiffTypes", "Expected arguments of type 'class'."
        );
        return OBJ_VAL(exc);
    }

    jml_obj_class_t    *sub_obj         = AS_CLASS(sub);
    jml_obj_class_t    *super_obj       = AS_CLASS(super);

    if (sub_obj == super_obj)
        return TRUE_VAL;

    jml_obj_class_t    *next            = sub_obj->super;
    while (next != NULL) {

        if (next == super_obj)
            return TRUE_VAL;

        next = next->super;
    }

    return FALSE_VAL;
}


/*core function registration*/
jml_module_function core_functions[] = {
    {"clock",                       &jml_core_clock},
    {"print",                       &jml_core_print},
    {"printfmt",                    &jml_core_print_fmt},
    {"reverse",                     &jml_core_reverse},
    {"size",                        &jml_core_size},
    {"instance",                    &jml_core_instance},
    {"subclass",                    &jml_core_subclass}
};


void
jml_core_register(void)
{
    int length = (sizeof(core_functions) / sizeof(core_functions[0]));

    for (int i = 0; i < length; ++i) {
        jml_module_function current = core_functions[i];
        jml_cfunction_register(current.name, current.function);
    }
}
