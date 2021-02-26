#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <jml.h>

#include <jml_module.h>
#include <jml_vm.h>
#include <jml_value.h>
#include <jml_util.h>
#include <jml_gc.h>
#include <jml_string.h>


static jml_value_t
jml_core_format(int arg_count, jml_value_t *args)
{
    if (arg_count == 0)
        return OBJ_VAL(
            jml_obj_exception_new("FormatErr", "Expected format string.")
        );

    if (!IS_STRING(args[0]))
        return OBJ_VAL(
            jml_obj_exception_new("FormatErr", "Expected format string.")
        );

    jml_obj_string_t *fmt_obj   = AS_STRING(args[0]);
    char             *fmt_str   = jml_strdup(fmt_obj->chars);
    int32_t           fmt_args  = 0;
    int32_t           fmt_extra = 0;
    int32_t           fmt_err   = 0;

    size_t            size      = (fmt_obj->length + 1) + (arg_count - 1) * 16;
    size_t            dest_size = size;

    char             *buffer    = jml_alloc(size);
    char             *save      = NULL;
    char             *token     = jml_strtok(fmt_str, "{}", &save);
    char             *last      = token;

    while (token != NULL) {
        if (fmt_args + 1 >= arg_count)
            ++fmt_extra;
        else {
            char *value_str     = jml_value_stringify(args[fmt_args + 1]);

            dest_size          += strlen(value_str) + strlen(token);
            REALLOC(char, buffer, size, dest_size);

            jml_strcat(jml_strcat(buffer, token), value_str);

            jml_free(value_str);
        }

        ++fmt_args;
        last                    = token;
        token                   = jml_strtok(NULL, "{}", &save);
    }


    fmt_err                     = fmt_args;

    if (fmt_obj->length < 2
        || !(fmt_obj->chars[fmt_obj->length - 1] == '}'
        && fmt_obj->chars[fmt_obj->length - 2] == '{')) {

        --fmt_err;
        --fmt_extra;
        jml_strcat(buffer, last);
    }

    jml_free(fmt_str);

    if (fmt_extra > 0 || fmt_err + 1 != arg_count) {
        jml_free(buffer);
        return OBJ_VAL(
            jml_obj_exception_format(
                "FormatErr",
                "Expected '%d' format arguments but got '%d'.",
                fmt_err, arg_count - 1
            )
        );
    }

    return OBJ_VAL(
        jml_obj_string_take(buffer, strlen(buffer))
    );
}


static jml_value_t
jml_core_print(int arg_count, jml_value_t *args)
{
    if (arg_count == 0) {
        printf("\n");
        return NONE_VAL;
    }

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
    if (arg_count == 0) {
        printf("\n");
        return NONE_VAL;
    }

    jml_value_t fmt = jml_core_format(arg_count, args);

    if (IS_STRING(fmt)) {
        printf("%s\n", AS_CSTRING(fmt));
        return NONE_VAL;
    }

    return fmt;
}


static jml_value_t
jml_core_reverse(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value = args[0];

    if (IS_STRING(value)) {
        jml_obj_string_t *obj   = AS_STRING(value);

        char *str               = jml_strdup(obj->chars);
        int length              = obj->length;
        char *end               = str + length;

        for (int i = 0; i < length / 2; ++i) {
            char temp           = str[i];
            str[i]              = str[length - 1 - i];
            str[length - 1 - i] = temp;
        }

        while(str < --end) {
            char temp;
            switch((*end & 0xf0) >> 4) {
                case 0x0f:
                    if (end - str < 4)
                        return false;

                    temp        = *(end - 3);
                    *(end - 3)  = *end;
                    *end        = temp;

                    temp        = *(end - 2);
                    *(end - 2)  = *(end - 1);
                    *(end - 1)  = temp;

                    end -= 3;
                    break;

                case 0x0e:
                    if (end - str < 3)
                        return false;

                    temp        = *(end - 2);
                    *(end - 2)  = *end;
                    *end        = temp;

                    end -= 2;
                    break;

                case 0x0c:
                case 0x0d:
                    if (end - str < 1)
                        return false;

                    temp        = *(end - 1);
                    *(end - 1)  = *end;
                    *end        = temp;

                    --end;
                    break;
            }
        }

        return OBJ_VAL(
            jml_obj_string_take(str, length)
        );

    } else if (IS_ARRAY(value)) {
        jml_obj_array_t *array          = AS_ARRAY(value);

        for (int i = 0; i < array->values.count / 2; ++i) {
            jml_value_t temp            = array->values.values[i];
            int pos = array->values.count - 1 - i;

            array->values.values[i]     = array->values.values[pos];
            array->values.values[pos]   = temp;
        }

        return OBJ_VAL(array);
    }

    return OBJ_VAL(
        jml_error_implemented(value)
    );
}


static jml_value_t
jml_core_size(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc            = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value = args[0];

    if (!IS_OBJ(value))
        goto err;

    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            return NUM_VAL(AS_STRING(value)->length);

        case OBJ_ARRAY:
            return NUM_VAL(AS_ARRAY(value)->values.count);

        case OBJ_INSTANCE: {
            jml_obj_instance_t *instance    = AS_INSTANCE(value);
            jml_value_t  last               = NONE_VAL;

            if (jml_vm_invoke_cstack(instance, vm->size_string, 0, &last))
                return last;

            return OBJ_VAL(jml_obj_exception_format(
                "DiffTypes",
                "Can't get size from instance of '%s'.",
                instance->klass->name->chars
            ));
        }

        default:
            goto err;
    }

err:
    return OBJ_VAL(
        jml_error_implemented(value)
    );
}


static jml_value_t
jml_core_char(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value   = args[0];

    if (IS_NUM(value)) {
        char c[5] = { 0 };
        if (jml_string_encode(c, (uint32_t)AS_NUM(value)))
            return jml_string_intern(c);

        return OBJ_VAL(
            jml_error_value("unicode codepoint")
        );
    }

    if (IS_STRING(value)) {
        jml_obj_string_t *string = AS_STRING(value);

        if (jml_string_len(string->chars, string->length) != 1) {
            return OBJ_VAL(
                jml_error_value("string length")
            );
        }

        uint32_t code = 0;
        if (jml_string_decode(string->chars, &code))
            return NUM_VAL(code);

        return OBJ_VAL(
            jml_error_value("unicode codepoint")
        );
    }

    return OBJ_VAL(
        jml_error_implemented(value)
    );
}


static jml_value_t
jml_core_instance(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t instance                = args[0];
    jml_value_t klass                   = args[1];

    if (!IS_INSTANCE(instance) || !IS_CLASS(klass)) {
        return OBJ_VAL(
            jml_error_types(false, 2, "instance", "class")
        );
    }

    jml_obj_instance_t *instance_obj    = AS_INSTANCE(instance);
    jml_obj_class_t    *klass_obj       = AS_CLASS(klass);

    return BOOL_VAL(instance_obj->klass == klass_obj);
}


static jml_value_t
jml_core_subclass(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc        = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t sub                 = args[0];
    jml_value_t super               = args[1];

    if (!IS_CLASS(sub) || !IS_CLASS(super)) {
        return OBJ_VAL(
            jml_error_types(false, 2, "class", "class")
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
    jml_obj_exception_t *exc        = jml_error_args(
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
    jml_obj_exception_t *exc        = jml_error_args(
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
    jml_obj_exception_t *exc        = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value = args[0];

    if (!IS_OBJ(value))
        goto err;

    switch (OBJ_TYPE(value)) {
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
        jml_error_implemented(value)
    );
}


static jml_value_t
jml_core_max(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc        = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t a = args[0];
    jml_value_t b = args[1];


    if (!IS_NUM(a) || !IS_NUM(b)) {
        return OBJ_VAL(
            jml_error_types(false, 2, "number", "number")
        );
    }

    return AS_NUM(a) >= AS_NUM(b) ? a : b;
}


static jml_value_t
jml_core_min(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc        = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t a = args[0];
    jml_value_t b = args[1];


    if (!IS_NUM(a) || !IS_NUM(b)) {
        return OBJ_VAL(
            jml_error_types(false, 2, "number", "number")
        );
    }

    return AS_NUM(a) < AS_NUM(b) ? a : b;
}


static jml_value_t
jml_core_assert(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc        = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    if (jml_value_falsey(args[0])) {
        return OBJ_VAL(jml_obj_exception_new(
            "AssertErr", "Assertion failed."
        ));
    }

    return NONE_VAL;
}


/*core table*/
static jml_module_function core_table[] = {
    {"format",                      &jml_core_format},
    {"print",                       &jml_core_print},
    {"printfmt",                    &jml_core_print_fmt},
    {"char",                        &jml_core_char},
    {"reverse",                     &jml_core_reverse},
    {"size",                        &jml_core_size},
    {"instance",                    &jml_core_instance},
    {"subclass",                    &jml_core_subclass},
    {"type",                        &jml_core_type},
    {"globals",                     &jml_core_globals},
    {"attr",                        &jml_core_attr},
    {"max",                         &jml_core_max},
    {"min",                         &jml_core_min},
    {"assert",                      &jml_core_assert},
    {NULL,                          NULL}
};


void
jml_core_register(void)
{
    jml_obj_module_t *core_module = jml_obj_module_new(
        vm->core_string, NULL);

    jml_module_register(core_module, core_table);
    jml_hashmap_set(&vm->modules, vm->core_string, OBJ_VAL(core_module));
    jml_hashmap_add(&core_module->globals, &vm->globals);
}
