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


/*helper functions*/
jml_obj_exception_t *
jml_core_exception_args(int arg_count, int expected_arg)
{
    char message[50];

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
jml_core_exception_implemented(jml_value_t value)
{
    char message[26];

    sprintf(message,  "Not implemented for %s.",
        jml_value_stringify_type(value));

    return jml_obj_exception_new(
        "NotImplemented", message
    );
}


jml_obj_exception_t *
jml_core_exception_types(bool mult, int arg_count, ...)
{
    va_list types;
    va_start(types, arg_count);

    size_t size = (arg_count + 1) * 32;
    char *message = jml_realloc(NULL, size);

    char *next = va_arg(types, char*);
    sprintf(message, "Expected arguments of <type %s>", next);

    for (int i = 1; i < arg_count; ++i) {

        char *next = va_arg(types, char*);
        char temp[32];
        if (mult)
            sprintf(temp, " or <type %s>", next);
        else
            sprintf(temp, " and <type %s>", next);

        size_t dest_size = strlen(message) + strlen(temp);
        REALLOC(char, message, size, dest_size);
        strcat(message, temp);
    }

    strcat(message, ".");

    jml_obj_exception_t *exc = jml_obj_exception_new(
        "DiffTypes", message
    );

    jml_realloc(message, 0);
    va_end(types);

    return exc;
}


/*core functions*/
static jml_value_t
jml_core_print(int arg_count, jml_value_t *args)
{
    for (int i = 0; i < arg_count; ++i) {
        if (IS_STRING(args[i]))
            printf("%s", AS_CSTRING(args[i]));
        else
            jml_value_print(args[i]);
        printf("\n");
    }

    return NONE_VAL;
}


static jml_value_t
jml_core_print_fmt(int arg_count, jml_value_t *args)
{
    jml_value_t fmt_value       = args[0];

    if (!IS_STRING(fmt_value)) {
        return OBJ_VAL(
            jml_core_exception_types(false, 1, "string")
        );
    }

    jml_obj_string_t *fmt_obj   = AS_STRING(fmt_value);
    char             *fmt_str   = jml_strdup(fmt_obj->chars);
    int               fmt_args  = 0;

    size_t size                 = fmt_obj->length + 16 * arg_count;
    char *string                = jml_alloc(size);

    char *token                 = jml_strtok(fmt_str, "{}");
    while (token != NULL) {

        char *value_str         = jml_value_stringify(args[fmt_args + 1]);

        size_t dest_size        = strlen(string) + strlen(value_str) + strlen(token);
        REALLOC(char, string, size, dest_size);

        strcat(string, token);
        strcat(string, value_str);

        jml_free(value_str);
        token = jml_strtok(NULL, "{}");
        ++fmt_args;
    }

    jml_free(token);
    jml_free(fmt_str);

    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, fmt_args);

    if (exc != NULL) {
        jml_free(string);
        return OBJ_VAL(exc);
    } else {
        int length = strlen(string) - 1;
        printf("%.*s\n", length, string);
        jml_free(string);
    }

    return NONE_VAL;
}


static jml_value_t
jml_core_string_fmt(int arg_count, jml_value_t *args)
{
    jml_value_t fmt_value       = args[0];

    if (!IS_STRING(fmt_value)) {
        return OBJ_VAL(
            jml_core_exception_types(false, 1, "string")
        );
    }

    jml_obj_string_t *fmt_obj   = AS_STRING(fmt_value);
    char             *fmt_str   = jml_strdup(fmt_obj->chars);
    int               fmt_args  = 0;

    size_t size                 = fmt_obj->length + 16 * arg_count;
    char *string                = jml_alloc(size);

    char *token                 = jml_strtok(fmt_str, "{}");
    while (token != NULL) {

        char *value_str         = jml_value_stringify(args[fmt_args + 1]);

        size_t dest_size        = strlen(string) + strlen(value_str) + strlen(token);
        REALLOC(char, string, size, dest_size);

        strcat(string, token);
        strcat(string, value_str);

        jml_free(value_str);
        token = jml_strtok(NULL, "{}");
        ++fmt_args;
    }

    jml_free(token);
    jml_free(fmt_str);

    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, fmt_args);

    if (exc != NULL) {
        jml_free(string);
        return OBJ_VAL(exc);
    }

    int length = strlen(string) - 1;

    jml_obj_string_t *formatted = jml_obj_string_copy(
        string, length);

    jml_free(string);

    return OBJ_VAL(formatted);
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

        char *str               = jml_strdup(string_obj->chars);
        int length              = string_obj->length;

        for (int i = 0; i < length / 2; ++i) {
            char temp           = str[i];
            str[i]              = str[length - 1 - i];
            str[length - 1 - i] = temp;
        }

        return OBJ_VAL(jml_obj_string_take(
            str, strlen(str))
        );

    } else if (IS_ARRAY(value)) {
        jml_obj_array_t *array  = AS_ARRAY(value);

        for (int i = 0; i < array->values.count / 2; ++i) {
            jml_value_t temp            = array->values.values[i];
            int pos = array->values.count - 1 - i;

            array->values.values[i]     = array->values.values[pos];
            array->values.values[pos]   = temp;
        }

        return OBJ_VAL(array);
    }

    return OBJ_VAL(
        jml_core_exception_types(true, 2, "string", "array")
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

    if (!IS_OBJ(value))
        goto err;

    switch (AS_OBJ(value)->type) {
        case OBJ_STRING:
            return NUM_VAL(AS_STRING(value)->length);

        case OBJ_ARRAY:
            return NUM_VAL(AS_ARRAY(value)->values.count);

        default:
            goto err;
    }

err:
    return OBJ_VAL(
        jml_core_exception_implemented(value)
    );
}


static jml_value_t
jml_core_char(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value   = args[0];

    if (IS_NUM(value)) {
        char chr[2];
        chr[0]          = (char)AS_NUM(value);
        chr[1]          = '\0';

        return jml_string_intern(chr);
    }

    return OBJ_VAL(
        jml_core_exception_types(false, 1, "number")
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
            jml_core_exception_types(false, 2, "instance", "class")
        );
    }

    jml_obj_instance_t *instance_obj    = AS_INSTANCE(instance);
    jml_obj_class_t    *klass_obj       = AS_CLASS(klass);

    return BOOL_VAL(instance_obj->klass == klass_obj);
}


static jml_value_t
jml_core_subclass(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc        = jml_core_exception_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t sub                 = args[0];
    jml_value_t super               = args[1];

    if (!IS_CLASS(sub) || !IS_CLASS(super)) {
        return OBJ_VAL(
            jml_core_exception_types(false, 2, "class", "class")
        );
    }

    jml_obj_class_t *sub_obj        = AS_CLASS(sub);
    jml_obj_class_t *super_obj      = AS_CLASS(super);

    if (sub_obj == super_obj)
        return TRUE_VAL;

    jml_obj_class_t *next           = sub_obj->super;
    while (next != NULL) {
        if (next == super_obj)
            return TRUE_VAL;

        next = next->super;
    }

    return FALSE_VAL;
}


static jml_value_t
jml_core_type(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc        = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return jml_string_intern(
        jml_value_stringify_type(args[0])
    );
}


static jml_value_t
jml_core_globals(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc        = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_obj_map_t *map              = jml_obj_map_new();

    jml_vm_push(OBJ_VAL(map));
    jml_hashmap_add(&vm->globals, &map->hashmap);
    return jml_vm_pop();
}


static jml_value_t
jml_core_attr(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc        = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value = args[0];

    if (!IS_OBJ(value))
        goto err;

    switch (AS_OBJ(value)->type) {
        case OBJ_MODULE: {
            jml_obj_map_t *map      = jml_obj_map_new();
            jml_vm_push(OBJ_VAL(map));
            jml_hashmap_add(&AS_MODULE(value)->globals, &map->hashmap);
            return jml_vm_pop();
        }

        case OBJ_CLASS: {
            jml_obj_map_t *map      = jml_obj_map_new();
            jml_vm_push(OBJ_VAL(map));
            jml_hashmap_add(&AS_CLASS(value)->methods, &map->hashmap);
            return jml_vm_pop();
        }

        case OBJ_INSTANCE: {
            jml_obj_map_t *map      = jml_obj_map_new();
            jml_vm_push(OBJ_VAL(map));
            jml_hashmap_add(&AS_INSTANCE(value)->fields, &map->hashmap);
            jml_hashmap_add(&AS_INSTANCE(value)->klass->methods, &map->hashmap);
            return jml_vm_pop();
        }

        default:
            goto err;
    }

err:
    return OBJ_VAL(
        jml_core_exception_implemented(value)
    );
}


/*core function registration*/
jml_module_function core_functions[] = {
    {"print",                       &jml_core_print},
    {"printfmt",                    &jml_core_print_fmt},
    {"stringfmt",                   &jml_core_string_fmt},
    {"char",                        &jml_core_char},
    {"reverse",                     &jml_core_reverse},
    {"size",                        &jml_core_size},
    {"instance",                    &jml_core_instance},
    {"subclass",                    &jml_core_subclass},
    {"type",                        &jml_core_type},
    {"globals",                     &jml_core_globals},
    {"attr",                        &jml_core_attr},
    {NULL,                          NULL}
};


void
jml_core_register(void)
{
    jml_module_function *current = core_functions;

    while (current->function != NULL) {
        jml_cfunction_register(
            current->name,
            current->function,
            NULL
        );

        ++current;
    }
}
