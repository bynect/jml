#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

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


static jml_obj_exception_t *
jml_core_exception_implemented(jml_value_t value)
{
    char message[26];

    sprintf(message,  "Not implemented for '%s'.",
        jml_obj_type_stringify(AS_OBJ(value)->type));

    return jml_obj_exception_new(
        "NotImplemented", message
    );
}


static jml_obj_exception_t *
jml_core_exception_types(int arg_count, ...)
{
    va_list types;
    va_start(types, arg_count);

    char buffer[512];
    char *message = buffer;

    char *next = va_arg(types, char*);
    sprintf(buffer, "Expected arguments of type '%s'", next);

    for (int i = 1; i < arg_count; ++i) {
        char *next = va_arg(types, char*);
        char temp[32];
        sprintf(temp, " and '%s'", next);

        message = jml_strcat(message, temp);
    }
    message = jml_strcat(message, ".");

    jml_obj_exception_t *exc = jml_obj_exception_new(
        "DiffTypes", buffer
    );

    va_end(types);

    return exc;
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
jml_core_timestamp(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL((double)time(NULL));
}


static jml_value_t
jml_core_localtime(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    time_t rawtime;
    time(&rawtime);

    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);

    char *local = asctime(timeinfo);
    return OBJ_VAL(jml_obj_string_take(
        local, strlen(local)));
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
        return OBJ_VAL(
            jml_core_exception_types(1, "string")
        );
    }

    jml_obj_string_t *fmt_obj   = AS_STRING(fmt_value);
    char             *fmt_str   = jml_strdup(fmt_obj->chars);
    int               fmt_args  = 0;

    char dest[2048] = "";
    char *ptr = dest;

    char *token = jml_strtok(fmt_str, "{}");
    while (token != NULL) {
        ptr = jml_strcat(ptr, token);

        char *value_str = jml_value_stringify(args[fmt_args + 1]);
        ptr = jml_strcat(ptr, value_str);

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
        arg_count, 1);

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

    return OBJ_VAL(
        jml_core_exception_implemented(value)
    );
}


static jml_value_t
jml_core_size(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value = args[0];

    if (IS_STRING(value))
        return NUM_VAL(AS_STRING(value)->length);

    return OBJ_VAL(
        jml_core_exception_implemented(value)
    );
}


static jml_value_t
jml_core_char(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value = args[0];

    if(IS_NUM(value)) {
        char chr[2];
        chr[0] = (char)AS_NUM(value);
        chr[1] = '\0';

        return OBJ_VAL(
            jml_obj_string_copy(chr, 2)
        );
    }

    return OBJ_VAL(
        jml_core_exception_types(1, "number")
    );
}


static jml_value_t
jml_core_instance(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t instance                = args[0];
    jml_value_t klass                   = args[1];

    if (!IS_INSTANCE(instance) || !IS_CLASS(klass)) {
        return OBJ_VAL(
            jml_core_exception_types(2, "instance", "class")
        );
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
        return OBJ_VAL(
            jml_core_exception_types(2, "class", "class")
        );
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
    {"time",                        &jml_core_timestamp},
    {"localtime",                   &jml_core_localtime},
    {"print",                       &jml_core_print},
    {"printfmt",                    &jml_core_print_fmt},
    {"char",                        &jml_core_char},
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
