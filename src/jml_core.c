#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <jml.h>

#include <jml/jml_module.h>
#include <jml/jml_value.h>
#include <jml/jml_util.h>
#include <jml/jml_gc.h>
#include <jml/jml_string.h>

#define JML_VM_INTERNAL
#include <jml/jml_vm.h>


static jml_value_t
jml_core_format_internal(jml_value_t format,
    int arg_count, jml_value_t *args)
{
    jml_obj_string_t *fmt_obj   = AS_STRING(format);
    char             *fmt_str   = jml_strdup(fmt_obj->chars);
    int32_t           fmt_args  = 0;
    int32_t           fmt_extra = 0;
    int32_t           fmt_err   = 0;

    size_t            size      = (fmt_obj->length + 1) + arg_count * 16;
    size_t            dest_size = size;

    char             *buffer    = jml_alloc(size);
    char             *save      = NULL;
    char             *token     = jml_strtok(fmt_str, "{}", &save);
    char             *last      = token;

    while (token != NULL) {
        if (fmt_args >= arg_count)
            ++fmt_extra;
        else {
            char *value_str     = jml_value_stringify(args[fmt_args]);

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

    if (fmt_extra > 0 || fmt_err != arg_count) {
        jml_free(buffer);
        return OBJ_VAL(
            jml_obj_exception_format(
                "FormatErr",
                "Expected '%d' format arguments but got '%d'.",
                fmt_err, arg_count
            )
        );
    }

    return OBJ_VAL(
        jml_obj_string_take(buffer, strlen(buffer))
    );
}


static jml_value_t
jml_core_format(int arg_count, jml_value_t *args)
{
    if (arg_count == 0) {
        return OBJ_VAL(
            jml_obj_exception_new("FormatErr", "Expected format string.")
        );
    }

    if (!IS_STRING(args[0])) {
        return OBJ_VAL(
            jml_obj_exception_new("FormatErr", "Expected format string.")
        );
    }

    return jml_core_format_internal(
        args[0], arg_count - 1, args + 1
    );
}


static jml_value_t
jml_core_format_array(int arg_count, jml_value_t *args)
{
    if (arg_count == 0) {
        return OBJ_VAL(
            jml_obj_exception_new("FormatErr", "Expected format string.")
        );
    }

    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        return OBJ_VAL(exc);

    if (!IS_STRING(args[0]) || !IS_ARRAY(args[1])) {
        return OBJ_VAL(
            jml_error_types(false, 2, "string", "array")
        );
    }

    jml_obj_array_t *array = AS_ARRAY(args[1]);
    return jml_core_format_internal(
        args[0],
        array->values.count,
        array->values.values
    );
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
        printf("%.*s", (int32_t)AS_STRING(fmt)->length, AS_CSTRING(fmt));
        return NONE_VAL;
    }

    return fmt;
}


static jml_value_t
jml_core_print_ln(int arg_count, jml_value_t *args)
{
    if (arg_count == 0) {
        printf("\n");
        return NONE_VAL;
    }

    for (int i = 0; i < arg_count; ++i) {
        if (IS_STRING(args[i]))
            printf("%.*s", (int32_t)AS_STRING(args[i])->length, AS_CSTRING(args[i]));
        else
            jml_value_print(args[i]);

        printf("\n");
    }

    return NONE_VAL;
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
            printf("%.*s", (int32_t)AS_STRING(args[i])->length, AS_CSTRING(args[i]));
        else
            jml_value_print(args[i]);
    }

    return NONE_VAL;
}


static jml_value_t
jml_core_repr(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    char *repr = jml_value_stringify(args[0]);
    return OBJ_VAL(
        jml_obj_string_take(repr, strlen(repr))
    );
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

        case OBJ_MAP:
            return NUM_VAL(AS_MAP(value)->hashmap.count);

        case OBJ_INSTANCE: {
            jml_obj_instance_t *instance        = AS_INSTANCE(value);
            jml_value_t *method;
            if (jml_hashmap_get(&instance->klass->statics,
                vm->size_string, &method)) {

                jml_value_t last                = NONE_VAL;
                jml_obj_coroutine_t *coroutine  = jml_obj_coroutine_new(NULL);

                if (IS_CFUNCTION(*method)) {
                    *coroutine->stack_top++     = OBJ_VAL(instance);

                    jml_vm_call_value(coroutine, *method, 1);
                    if (jml_vm_call_coroutine(coroutine, &last) == INTERPRET_OK)
                        return last;

                } else {
                    jml_vm_call_value(coroutine, *method, 0);
                    if (jml_vm_call_coroutine(coroutine, &last) == INTERPRET_OK)
                        return last;
                }
            }

            return OBJ_VAL(jml_obj_exception_format(
                "DiffTypes",
                "Can't get size from instance of '%.*s'.",
                instance->klass->name->length,
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
    jml_obj_exception_t *exc        = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value               = args[0];

    if (IS_NUM(value)) {
        char c[5] = { 0 };
        if (jml_string_encode(c, (uint32_t)AS_NUM(value)))
            return jml_string_intern(c);

        return OBJ_VAL(
            jml_error_value("unicode codepoint")
        );
    }

    if (IS_STRING(value)) {
        jml_obj_string_t *string    = AS_STRING(value);

        if (jml_string_len(string->chars, string->length) != 1) {
            return OBJ_VAL(
                jml_error_value("string length")
            );
        }

        uint32_t code               = 0;
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
    jml_obj_exception_t *exc            = jml_error_args(
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
    jml_gc_exempt_push(OBJ_VAL(map));

    if (vm->current == NULL) {
        jml_hashmap_add(&vm->globals, &map->hashmap);
    } else {
        jml_hashmap_add(&vm->current->globals, &map->hashmap);
        jml_hashmap_add(&vm->builtins, &map->hashmap);
    }

    return jml_gc_exempt_pop();
}


static jml_value_t
jml_core_attr(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc        = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        return OBJ_VAL(exc);

    jml_value_t value               = args[0];

    if (!IS_OBJ(value))
        goto err;

    switch (OBJ_TYPE(value)) {
        case OBJ_MODULE: {
            jml_obj_map_t *map      = jml_obj_map_new();
            jml_gc_exempt_push(OBJ_VAL(map));
            jml_hashmap_add(&AS_MODULE(value)->globals, &map->hashmap);
            return jml_gc_exempt_pop();
        }

        case OBJ_CLASS: {
            jml_obj_map_t *map      = jml_obj_map_new();
            jml_gc_exempt_push(OBJ_VAL(map));
            jml_hashmap_add(&AS_CLASS(value)->statics, &map->hashmap);
            return jml_gc_exempt_pop();
        }

        case OBJ_INSTANCE: {
            jml_obj_map_t *map      = jml_obj_map_new();
            jml_gc_exempt_push(OBJ_VAL(map));
            jml_hashmap_add(&AS_INSTANCE(value)->fields, &map->hashmap);
            jml_hashmap_add(&AS_INSTANCE(value)->klass->statics, &map->hashmap);
            return jml_gc_exempt_pop();
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
    char *message = "Assertion failed.";

    switch (arg_count) {
        case 0: {
            return OBJ_VAL(
                jml_obj_exception_new("TooFewArgs", "Expected assert condition.")
            );
        }

        case 2: {
            if (!IS_STRING(args[1]))
                jml_obj_exception_new("DiffTypes", "Expected assert message.");
        } /*fallthrough*/

        case 1: {
            if (jml_value_falsey(args[0]))
                return OBJ_VAL(jml_obj_exception_new("AssertErr", message));
            else
                break;
        }

        default: {
            return OBJ_VAL(
                jml_obj_exception_new(
                    "TooManyArgs", "Expected assert condition and message."
                )
            );
        }
    }

    return NONE_VAL;
}


static jml_value_t
jml_core_exception(int arg_count, jml_value_t *args)
{
    char *name      = arg_count > 0 && IS_STRING(args[0]) ? AS_CSTRING(args[0]) : "";
    char *message   = arg_count > 1 && IS_STRING(args[1]) ? AS_CSTRING(args[1]) : "";

    return OBJ_VAL(
        jml_obj_exception_new(name, message)
    );
}


/*core table*/
static jml_module_function core_table[] = {
    {"format",                      &jml_core_format},
    {"format_array",                &jml_core_format_array},
    {"printfmt",                    &jml_core_print_fmt},
    {"println",                     &jml_core_print_ln},
    {"print",                       &jml_core_print},
    {"repr",                        &jml_core_repr},
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
    {"__exception",                 &jml_core_exception},
    {NULL,                          NULL}
};


/*glue code*/
static const char core_glue[] = "\
fn exception(name, msg) {\n\
    let exc = try __exception(name, msg)\n\
    exc\n\
}\n\
\n\
class Abstract {\n\
    fn __init(^_) {\n\
        __exception(\n\
            \"AbstractErr\", \"Can't instantiate Abstract class\"\n\
        )\n\
    }\n\
}\n\
";


void
jml_core_register(jml_vm_t *vm)
{
    jml_obj_string_t *core_string   = jml_obj_string_copy("core", 4);
    jml_gc_exempt_push(OBJ_VAL(core_string));

    jml_obj_module_t *core_module   = jml_obj_module_new(core_string, NULL);
    jml_gc_exempt_push(OBJ_VAL(core_module));

    jml_module_register(core_module, core_table);
    jml_hashmap_set(&vm->modules, core_string, jml_gc_exempt_peek(0));
    jml_hashmap_free(&vm->globals);

    jml_obj_function_t *main = jml_compiler_compile(core_glue, core_module, false);
    JML_ASSERT(main != NULL, "");
    jml_gc_exempt_push(OBJ_VAL(main));

    jml_obj_closure_t *closure = jml_obj_closure_new(main);
    jml_gc_exempt_push(OBJ_VAL(closure));

    jml_obj_coroutine_t *coroutine = jml_obj_coroutine_new(closure);
    jml_gc_exempt_push(OBJ_VAL(coroutine));

    jml_obj_module_t *super = vm->current;
    vm->current = core_module;

    JML_UNUSED(jml_interpret_result result) = jml_vm_call_coroutine(coroutine, NULL);
    JML_ASSERT(result == INTERPRET_OK, "");

    jml_gc_exempt_pop();
    jml_gc_exempt_pop();
    jml_gc_exempt_pop();
    vm->current = super;

    jml_hashmap_add(&core_module->globals, &vm->builtins);
    jml_hashmap_add(&core_module->globals, &vm->globals);

    jml_gc_exempt_pop();
    jml_gc_exempt_pop();

    jml_obj_string_t *argv_string   = jml_obj_string_copy("argv", 4);
    jml_gc_exempt_push(OBJ_VAL(argv_string));
    jml_obj_string_t *argc_string   = jml_obj_string_copy("argc", 4);
    jml_gc_exempt_push(OBJ_VAL(argc_string));

    jml_obj_array_t *argv           = jml_obj_array_new();
    jml_gc_exempt_push(OBJ_VAL(argv));
    double argc = 0;

    if (vm->context != NULL) {
        for (int i = 0; i < vm->context->argc; ++i) {
            jml_obj_array_append(
                argv, jml_string_intern(vm->context->argv[i])
            );
        }
        argc = vm->context->argc;
    }

    jml_hashmap_set(&core_module->globals, argv_string, OBJ_VAL(argv));
    jml_hashmap_set(&core_module->globals, argc_string, NUM_VAL(argc));

    jml_gc_exempt_pop();
    jml_gc_exempt_pop();
    jml_gc_exempt_pop();
}
