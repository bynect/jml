#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <limits.h>

#include <jml/jml_repr.h>
#include <jml/jml_gc.h>

#define JML_VM_INTERNAL
#include <jml/jml_vm.h>


void
jml_value_print(jml_value_t value)
{
#ifdef JML_NAN_TAGGING
    if (IS_OBJ(value))
        jml_obj_print(value);

    else if (IS_NUM(value))
        printf("%.*g", DBL_DIG, AS_NUM(value));

    else if (IS_BOOL(value))
        printf(AS_BOOL(value) ? "true" : "false");

    else if (IS_NONE(value))
        printf("none");
#else
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;

        case VAL_NONE:
            printf("none");
            break;

        case VAL_NUM:
            printf("%.*g", DBL_DIG, AS_NUM(value));
            break;

        case VAL_OBJ:
            jml_obj_print(value);
            break;
    }
#endif
}


static void
jml_obj_function_print(jml_obj_function_t *function,
    const char *type)
{
    printf("<%s ", type);

    if (function->name == NULL) {
        printf("__main>");
        return;
    }

    if (function->module != NULL)
        printf("%.*s.", (int32_t)function->module->name->length,
            function->module->name->chars);

    if (function->klass_name != NULL)
        printf("%.*s.", (int32_t)function->klass_name->length,
            function->klass_name->chars);

    printf("%.*s", (int32_t)function->name->length,
        function->name->chars);

    if (!function->variadic)
        printf("/%d", function->arity);

    printf(">");
}


static void
jml_obj_cfunction_print(jml_obj_cfunction_t *function,
    const char *type)
{
    printf("<%s", type);

    if (function->name == NULL) {
        printf(">");
        return;
    }

    printf(" ");

    if (function->module != NULL)
        printf("%.*s.", (int32_t)function->module->name->length,
            function->module->name->chars);

    if (function->klass_name != NULL)
        printf("%.*s.", (int32_t)function->klass_name->length,
            function->klass_name->chars);

    printf("%.*s>", (int32_t)function->name->length,
        function->name->chars);
}


void
jml_obj_print(jml_value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: {
            jml_obj_string_t *string = AS_STRING(value);
            printf("\"%.*s\"", (int32_t)string->length, string->chars);
            break;
        }

        case OBJ_ARRAY: {
            printf("[");
            jml_value_array_t array = AS_ARRAY(value)->values;
            if (array.count <= 0) {
                printf("]");
                break;
            }

            int item_count          = array.count - 1;
            for (int i = 0; i < item_count; ++i) {
                jml_value_print(array.values[i]);
                printf(", ");
            }

            jml_value_print(array.values[item_count]);
            printf("]");
            break;
        }

        case OBJ_MAP: {
            printf("{");
            jml_hashmap_t hashmap   = AS_MAP(value)->hashmap;
            if (hashmap.count <= 0) {
                printf("}");
                break;
            }

            int item_count          = hashmap.count - 1;
            jml_hashmap_entry_t *entries = jml_hashmap_iterator(&hashmap);

            for (int i = 0; i < item_count; ++i) {
                printf("\"%.*s\": ", (int32_t)entries[i].key->length,
                    entries[i].key->chars);
                jml_value_print(entries[i].value);
                printf(", ");
            }

            printf("\"%.*s\": ",(int32_t) entries[item_count].key->length,
                entries[item_count].key->chars);
            jml_value_print(entries[item_count].value);
            printf("}");

            jml_realloc(entries, 0);
            break;
        }

        case OBJ_MODULE: {
            jml_obj_module_t *module = AS_MODULE(value);
            printf("<module %.*s>", (int32_t)module->name->length, module->name->chars);
            break;
        }

        case OBJ_CLASS: {
            jml_obj_class_t *klass  = AS_CLASS(value);
            printf("<class ");

            if (klass->module != NULL)
                printf("%.*s.", (int32_t)klass->module->name->length,
                    klass->module->name->chars);

            printf("%.*s>", (int32_t)klass->name->length, klass->name->chars);
            break;
        }

        case OBJ_INSTANCE: {
            jml_obj_instance_t *instance    = AS_INSTANCE(value);

            if (vm->print_string != NULL) {
                jml_value_t *method;
                if (jml_hashmap_get(&instance->klass->statics,
                    vm->print_string, &method)) {

                    if (IS_CFUNCTION(*method)) {
                        jml_value_t args = OBJ_VAL(instance);
                        AS_CFUNCTION(*method)->function(1, &args);
                    } else {
                        jml_obj_coroutine_t *coroutine = jml_obj_coroutine_new(NULL);
                        jml_vm_call_value(coroutine, *method, 0);
                        jml_vm_call_coroutine(coroutine, NULL);
                    }
                    return;
                }
            }

            printf("<instance of ");

            if (instance->klass->module != NULL)
                printf("%.*s.", (int32_t)instance->klass->module->name->length,
                    instance->klass->module->name->chars);

            printf("%.*s>", (int32_t)instance->klass->name->length,
                instance->klass->name->chars);
            break;
        }

        case OBJ_METHOD:
            jml_obj_function_print(
                AS_METHOD(value)->method->function, "fn");
            break;

        case OBJ_FUNCTION:
            jml_obj_function_print(
                AS_FUNCTION(value), "fn");
            break;

        case OBJ_CLOSURE:
            jml_obj_function_print(
                AS_CLOSURE(value)->function, "fn");
            break;

        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;

        case OBJ_COROUTINE:
            jml_obj_function_print(
                AS_COROUTINE(value)->frames[0].closure->function,
                "coroutine"
            );
            break;

        case OBJ_CFUNCTION:
            jml_obj_cfunction_print(
                AS_CFUNCTION(value), "builtin fn");
            break;

        case OBJ_EXCEPTION: {
            jml_obj_exception_t *exc = AS_EXCEPTION(value);
            printf("<exception ");

            if (exc->module != NULL)
                printf("%.*s.", (int32_t)exc->module->name->length,
                    exc->module->name->chars);

            printf("%.*s>", (int32_t)exc->name->length, exc->name->chars);
            break;
        }
    }
}


char *
jml_value_stringify(jml_value_t value)
{
#ifdef JML_NAN_TAGGING
    if (IS_BOOL(value))
        return jml_strdup(AS_BOOL(value) ? "true" : "false");

    else if (IS_NONE(value))
        return jml_strdup("none");

    else if (IS_NUM(value)) {
        char buffer[DBL_MANT_DIG];
        sprintf(buffer, "%.*g", DBL_DIG, AS_NUM(value));
        return jml_strdup(buffer);

    } else if (IS_OBJ(value))
        return jml_obj_stringify(value);
#else
    switch (value.type) {
        case VAL_BOOL:
            return jml_strdup(AS_BOOL(value) ? "true" : "false");

        case VAL_NONE:
            return jml_strdup("none");

        case VAL_NUM: {
            char buffer[DBL_MANT_DIG];
            sprintf(buffer, "%.*g", DBL_DIG, AS_NUM(value));
            return jml_strdup(buffer);
        }

        case VAL_OBJ:
            return jml_obj_stringify(value);
    }
#endif
    return NULL;
}


static char *
jml_obj_function_stringify(jml_obj_function_t *function)
{
    if (function->name == NULL) {
        return jml_strdup("<fn __main>");
    }

    size_t size = function->name->length * GC_HEAP_GROW_FACTOR;
    char *fn = jml_realloc(NULL, size);

    size_t pos = sprintf(fn, "<fn ");

    if (function->module != NULL) {
        REALLOC(char, fn, size, pos + function->module->name->length);
        pos += sprintf(fn + pos, "%.*s.", (int32_t)function->module->name->length,
            function->module->name->chars);
    }

    if (function->klass_name != NULL) {
        REALLOC(char, fn, size, pos + function->klass_name->length);
        pos += sprintf(fn + pos, "%.*s.", (int32_t)function->klass_name->length,
            function->klass_name->chars);
    }

    REALLOC(char, fn, size, pos + function->name->length + 10);
    pos += sprintf(fn + pos, "%.*s", (int32_t)function->name->length,
        function->name->chars);

    if (!function->variadic)
        pos += sprintf(fn + pos, "/%d", function->arity);

    pos += sprintf(fn + pos, ">");

    return fn;
}


static char *
jml_obj_cfunction_stringify(jml_obj_cfunction_t *function)
{
    if (function->name == NULL) {
        return jml_strdup("<builtin fn>");
    }

    size_t size = function->name->length * GC_HEAP_GROW_FACTOR;
    char *cfn = jml_realloc(NULL, size);

    size_t pos = sprintf(cfn, "<builtin fn ");

    if (function->module != NULL) {
        REALLOC(char, cfn, size, pos + function->module->name->length);
        pos += sprintf(cfn + pos, "%.*s.", (int32_t)function->module->name->length,
            function->module->name->chars);
    }

    if (function->klass_name != NULL) {
        REALLOC(char, cfn, size, pos + function->klass_name->length);
        pos += sprintf(cfn + pos, "%.*s.", (int32_t)function->klass_name->length,
            function->klass_name->chars);
    }

    REALLOC(char, cfn, size, pos + function->name->length + 3);
    pos += sprintf(cfn + pos, "%.*s>", (int32_t)function->name->length,
        function->name->chars);

    return cfn;
}


#define CLASS_NAME(buffer, klass, size, pos)            \
    do {                                                \
        if (klass->module != NULL) {                    \
            REALLOC(char, buffer, size,                 \
                pos + klass->module->name->length);     \
                                                        \
            pos += sprintf(buffer + pos, "%.*s.",       \
                (int32_t)klass->module->name->length,   \
                klass->module->name->chars);            \
        }                                               \
        REALLOC(char, buffer, size,                     \
            size + klass->name->length + 3);            \
                                                        \
        pos += sprintf(buffer + pos, "%.*s",            \
            (int32_t)klass->name->length,               \
            klass->name->chars);                        \
    } while (false)


static char *
jml_obj_class_stringify(jml_obj_class_t *klass)
{
    size_t size = klass->name->length * GC_HEAP_GROW_FACTOR;
    char *buffer = jml_realloc(NULL, size);

    size_t pos = sprintf(buffer, "<class ");
    CLASS_NAME(buffer, klass, size, pos);

    return buffer;
}


static char *
jml_obj_instance_stringify(jml_obj_instance_t *instance)
{
    if (vm->str_string != NULL) {
        jml_value_t *method;
        if (jml_hashmap_get(&instance->klass->statics,
            vm->str_string, &method)) {

            jml_value_t last                = NONE_VAL;

            if (IS_CFUNCTION(*method)) {
                jml_value_t args = OBJ_VAL(instance);
                last = AS_CFUNCTION(*method)->function(1, &args);
                return jml_value_stringify(last);

            } else {
                jml_obj_coroutine_t *coroutine  = jml_obj_coroutine_new(NULL);

                if (jml_vm_call_value(coroutine, *method, 0) &&
                    jml_vm_call_coroutine(coroutine, &last) == INTERPRET_OK)
                    return jml_value_stringify(last);
            }

            jml_vm_error(
                "DiffTypes: Can't get size from instance of '%.*s'.",
                (int32_t)instance->klass->name->length,
                instance->klass->name->chars
            );
        }
    }

    size_t size = instance->klass->name->length * GC_HEAP_GROW_FACTOR;
    char *buffer = jml_realloc(NULL, size);

    size_t pos = sprintf(buffer, "<instance of ");
    CLASS_NAME(buffer, instance->klass, size, pos);

    return buffer;
}


char *
jml_obj_stringify(jml_value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: {
            size_t length = AS_STRING(value)->length + 1;
            char *dest = jml_realloc(NULL, length);

            if (!dest)
                return NULL;

            return memcpy(dest, AS_CSTRING(value), length);
        }

        case OBJ_ARRAY: {
            jml_value_array_t array = AS_ARRAY(value)->values;

            if (array.count <= 0)
                return jml_strdup("[]");

            size_t size = array.count * 34 + 3;
            char *buffer = jml_realloc(NULL, size);
            char *ptr = buffer;
            *ptr++ = '[';

            int item_count          = array.count - 1;
            for (int i = 0; i < item_count; ++i) {
                char *temp = jml_value_stringify(array.values[i]);
                REALLOC(char, buffer, size, (ptr - buffer) + strlen(temp));
                ptr += sprintf(ptr, "%s, ", temp);
                jml_free(temp);
            }

            char *temp = jml_value_stringify(array.values[item_count]);
            REALLOC(char, buffer, size, (ptr - buffer) + strlen(temp));
            ptr += sprintf(ptr, "%s", temp);
            jml_free(temp);

            *ptr++ = ']';
            *ptr = 0;

            return buffer;
        }

        case OBJ_MAP: {
            jml_hashmap_t hashmap   = AS_MAP(value)->hashmap;

            if (hashmap.count <= 0)
                return jml_strdup("{}");

            size_t size = hashmap.count * 38 + 3;
            char *buffer = jml_realloc(NULL, size);
            char *ptr = buffer;
            *ptr++ = '{';

            int item_count          = hashmap.count - 1;
            jml_hashmap_entry_t *entries = jml_hashmap_iterator(&hashmap);

            for (int i = 0; i < item_count; ++i) {
                char *temp = jml_value_stringify(entries[i].value);
                REALLOC(char, buffer, size,
                    (ptr - buffer) + strlen(temp) + entries[i].key->length + 1);

                ptr += sprintf(ptr, "\"%.*s\": %s, ", (int32_t)entries[i].key->length,
                    entries[i].key->chars, temp);
                jml_free(temp);
            }

            char *temp = jml_value_stringify(entries[item_count].value);
            REALLOC(char, buffer, size,
                (ptr - buffer) + strlen(temp) + entries[item_count].key->length + 2);

            ptr += sprintf(ptr, "\"%.*s\": %s", (int32_t)entries[item_count].key->length,
                entries[item_count].key->chars, temp);
            jml_free(temp);
            jml_free(entries);

            *ptr++ = '}';
            *ptr = 0;

            return buffer;
        }

        case OBJ_MODULE: {
            jml_obj_module_t *module = AS_MODULE(value);
            char *buffer = jml_alloc(module->name->length + 16);

            sprintf(buffer, "<module %.*s>",
                (int32_t)module->name->length,
                module->name->chars
            );
            return buffer;
        }

        case OBJ_CLASS:
            return jml_obj_class_stringify(
                AS_CLASS(value));

        case OBJ_INSTANCE:
            return jml_obj_instance_stringify(
                AS_INSTANCE(value));

        case OBJ_METHOD:
            return jml_obj_function_stringify(
                AS_METHOD(value)->method->function);

        case OBJ_FUNCTION:
            return jml_obj_function_stringify(
                AS_FUNCTION(value));

        case OBJ_CLOSURE:
            return jml_obj_function_stringify(
                AS_CLOSURE(value)->function);

        case OBJ_UPVALUE:
            return jml_strdup("<upvalue>");

        case OBJ_COROUTINE:
            return jml_strdup("<coroutine>");

        case OBJ_CFUNCTION:
            return jml_obj_cfunction_stringify(
                AS_CFUNCTION(value));

        case OBJ_EXCEPTION: {
            jml_obj_exception_t *exc = AS_EXCEPTION(value);

            size_t size = exc->name->length + 32;
            if (exc->module != NULL)
                size += exc->module->name->length;

            char *buffer = jml_alloc(size);
            if (exc->module != NULL) {
                sprintf(buffer, "<exception %.*s.%.*s>",
                    (int32_t)exc->module->name->length,
                    exc->module->name->chars,
                    (int32_t)exc->name->length,
                    exc->name->chars
                );
            } else {
                sprintf(buffer, "<exception %.*s>",
                    (int32_t)exc->name->length,
                    exc->name->chars
                );
            }

            return buffer;
        }
    }
    return NULL;
}


const char *
jml_value_stringify_type(jml_value_t value)
{
#ifdef JML_NAN_TAGGING
    if (IS_BOOL(value))
        return "<type bool>";

    else if (IS_NONE(value))
        return "<type none>";

    else if (IS_NUM(value))
        return "<type number>";

    else if (IS_OBJ(value))
        return jml_obj_stringify_type(value);
#else
    switch (value.type) {
        case VAL_BOOL:
            return "<type bool>";

        case VAL_NONE:
            return "<type none>";

        case VAL_NUM:
            return "<type number>";

        case VAL_OBJ:
            return jml_obj_stringify_type(value);
    }
#endif
    return NULL;
}


const char *
jml_obj_stringify_type(jml_value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            return "<type string>";

        case OBJ_ARRAY:
            return "<type array>";

        case OBJ_MAP:
            return "<type map>";

        case OBJ_MODULE:
            return "<type module>";

        case OBJ_CLASS:
            return "<type class>";

        case OBJ_INSTANCE:
            return "<type instance>";

        case OBJ_METHOD:
            return "<type method>";

        case OBJ_FUNCTION:
            return "<type fn>";

        case OBJ_CLOSURE:
            return "<type fn>";

        case OBJ_UPVALUE:
            return "<type upvalue>";

        case OBJ_COROUTINE:
            return "<type coroutine>";

        case OBJ_CFUNCTION:
            return "<type builtin fn>";

        case OBJ_EXCEPTION:
            return "<type exception>";
    }
    return NULL;
}
