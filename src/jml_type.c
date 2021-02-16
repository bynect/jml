#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <jml_type.h>
#include <jml_gc.h>
#include <jml_vm.h>
#include <jml_util.h>


#define ALLOCATE_OBJ(type, obj_type)                    \
    (type*)jml_obj_allocate(sizeof(type), obj_type)


static jml_obj_t *
jml_obj_allocate(size_t size, jml_obj_type type)
{
    jml_obj_t *object           = jml_reallocate(
        NULL, 0, size);

    object->type                = type;
    object->marked              = false;
    object->next                = vm->objects;
    vm->objects                 = object;

#ifdef JML_TRACE_MEM
    printf(
        "[MEM] |%p allocate %zd (type %s)|\n",
        (void*)object,
        size,
        jml_obj_stringify_type(OBJ_VAL(object))
        );
#endif
  return object;
}


static jml_obj_string_t *
jml_obj_string_allocate(char *chars,
    size_t length, uint32_t hash)
{
    jml_obj_string_t *string    = ALLOCATE_OBJ(
        jml_obj_string_t, OBJ_STRING
    );
    string->length              = length;
    string->chars               = chars;
    string->hash                = hash;

    jml_vm_push(OBJ_VAL(string));
    jml_hashmap_set(&vm->strings, string, NONE_VAL);
    jml_vm_pop();

    return string;
}


static uint32_t
jml_obj_string_hash(const char *key, size_t length)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < (int)length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}


jml_obj_string_t *
jml_obj_string_take(char *chars, size_t length)
{
    uint32_t hash = jml_obj_string_hash(chars, length);
    jml_obj_string_t *interned = jml_hashmap_find(
        &vm->strings, chars, length, hash);

    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return jml_obj_string_allocate(chars, length, hash);
}


jml_obj_string_t *
jml_obj_string_copy(const char *chars, size_t length)
{
    uint32_t hash               = jml_obj_string_hash(
        chars, length);

    jml_obj_string_t *interned  = jml_hashmap_find(
        &vm->strings,chars, length, hash
    );

    if (interned != NULL) return interned;

    char *heap_chars            = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length]          = '\0';

    return jml_obj_string_allocate(heap_chars, length, hash);
}


jml_obj_array_t *
jml_obj_array_new(void)
{
    jml_obj_array_t *array  = ALLOCATE_OBJ(
        jml_obj_array_t, OBJ_ARRAY);

    jml_value_array_t value_array;
    jml_value_array_init(&value_array);

    array->values           = value_array;

    return array;
}


void
jml_obj_array_append(jml_obj_array_t *array,
    jml_value_t value)
{
    jml_vm_push(value);

    jml_value_array_write(&array->values,
        jml_vm_peek(0));

    jml_vm_pop();
}


void
jml_obj_array_add(jml_obj_array_t *source,
    jml_obj_array_t *dest)
{
    jml_vm_push(OBJ_VAL(source));
    jml_vm_push(OBJ_VAL(dest));

    for (int i = 0; i < source->values.count; ++i) {
        jml_obj_array_append(dest, source->values.values[i]);
    }

    jml_vm_pop_two();
}


jml_obj_map_t *
jml_obj_map_new(void)
{
    jml_obj_map_t *map  = ALLOCATE_OBJ(
        jml_obj_map_t, OBJ_MAP);

    jml_hashmap_t hashmap;
    jml_hashmap_init(&hashmap);

    map->hashmap        = hashmap;

    return map;
}


jml_obj_class_t *
jml_obj_class_new(jml_obj_string_t *name)
{
    jml_obj_class_t *klass  = ALLOCATE_OBJ(
        jml_obj_class_t, OBJ_CLASS);

    klass->name             = name;
    klass->super            = NULL;
    klass->inheritable      = true;
    klass->module           = NULL;

    jml_hashmap_init(&klass->methods);

    return klass;
}


jml_obj_instance_t *
jml_obj_instance_new(jml_obj_class_t *klass)
{
    jml_obj_instance_t *instance = ALLOCATE_OBJ(
        jml_obj_instance_t, OBJ_INSTANCE);

    instance->klass              = klass;
    instance->extra              = NULL;

    jml_hashmap_init(&instance->fields);

    return instance;
}


jml_obj_method_t *
jml_obj_method_new(jml_value_t receiver,
    jml_obj_closure_t *method)
{
    jml_obj_method_t *bound = ALLOCATE_OBJ(
        jml_obj_method_t, OBJ_METHOD);

    bound->receiver         = receiver;
    bound->method           = method;

    return bound;
}


jml_obj_closure_t *
jml_obj_closure_new(jml_obj_function_t *function)
{
    jml_obj_upvalue_t **upvalues = ALLOCATE(
        jml_obj_upvalue_t*, function->upvalue_count);

    for (int i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    jml_obj_closure_t *closure   = ALLOCATE_OBJ(
        jml_obj_closure_t, OBJ_CLOSURE);

    closure->function       = function;
    closure->upvalues       = upvalues;
    closure->upvalue_count  = function->upvalue_count;

    return closure;
}


jml_obj_upvalue_t *
jml_obj_upvalue_new(jml_value_t *slot)
{
    jml_obj_upvalue_t *upvalue  = ALLOCATE_OBJ(
        jml_obj_upvalue_t, OBJ_UPVALUE);

    upvalue->location           = slot;
    upvalue->closed             = NONE_VAL;
    upvalue->next               = NULL;

    return upvalue;
}


jml_obj_function_t *
jml_obj_function_new(void)
{
    jml_obj_function_t *function = ALLOCATE_OBJ(
        jml_obj_function_t, OBJ_FUNCTION);

    function->arity                 = 0;
    function->upvalue_count         = 0;
    function->name                  = NULL;
    function->klass_name            = NULL;
    function->module                = NULL;

    jml_bytecode_init(&function->bytecode);

    return function;
}


jml_obj_cfunction_t *
jml_obj_cfunction_new(jml_obj_string_t *name,
    jml_cfunction function, jml_obj_module_t *module)
{
    jml_obj_cfunction_t *cfunction  = ALLOCATE_OBJ(
        jml_obj_cfunction_t, OBJ_CFUNCTION);

    cfunction->name                 = name;
    cfunction->function             = function;
    cfunction->klass_name           = NULL;
    cfunction->module               = module;

    return cfunction;
}


jml_obj_exception_t *
jml_obj_exception_new(const char *name,
    const char *message)
{
    jml_vm_push(jml_string_intern(name));
    jml_vm_push(jml_string_intern(message));

    jml_obj_exception_t *exc = ALLOCATE_OBJ(
        jml_obj_exception_t, OBJ_EXCEPTION);

    exc->name               = AS_STRING(jml_vm_peek(1));
    exc->message            = AS_STRING(jml_vm_peek(0));
    exc->module             = NULL;

    jml_vm_pop_two();

    return exc;
}


jml_obj_exception_t *
jml_obj_exception_format(const char *name,
    char *message_format, ...)
{
    va_list args;
    va_start(args, message_format);

    size_t size     = strlen(message_format) * sizeof(char) * 32;
    char  *message  = jml_alloc(size);

    vsprintf(message, message_format, args);
    va_end(args);

    jml_obj_exception_t *exc = jml_obj_exception_new(
        name, message);

    jml_free(message);

    return exc;
}


jml_obj_module_t *
jml_obj_module_new(jml_obj_string_t *name, void *handle)
{
    jml_obj_module_t *module = ALLOCATE_OBJ(
        jml_obj_module_t, OBJ_MODULE);

    module->name    = name;
    module->handle  = handle;

    jml_hashmap_init(&module->globals);
    jml_value_array_init(&module->saved);

    return module;
}


static void
jml_obj_function_print(jml_obj_function_t *function)
{
    printf("<fn ");

    if (function->name == NULL) {
        printf("__main>");
        return;
    }

    if (function->module != NULL)
        printf("%s.", function->module->name->chars);

    if (function->klass_name != NULL)
        printf("%s.", function->klass_name->chars);

    printf("%s/%d>", function->name->chars, function->arity);
}


static void
jml_obj_cfunction_print(jml_obj_cfunction_t *function)
{
    printf("<builtin fn");

    if (function->name == NULL) {
        printf(">");
        return;
    }

    printf(" ");

    if (function->module != NULL)
        printf("%s.", function->module->name->chars);

    if (function->klass_name != NULL)
        printf("%s.", function->klass_name->chars);

    printf("%s>", function->name->chars);
}


void
jml_obj_print(jml_value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("\"%s\"", AS_CSTRING(value));
            break;

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
                printf("\"%s\": ", entries[i].key->chars);
                jml_value_print(entries[i].value);
                printf(", ");
            }

            printf("\"%s\": ", entries[item_count].key->chars);
            jml_value_print(entries[item_count].value);
            printf("}");

            jml_realloc(entries, 0);
            break;
        }

        case OBJ_MODULE:
            printf("<module %s>", AS_MODULE(value)->name->chars);
            break;

        case OBJ_CLASS: {
            jml_obj_class_t *klass = AS_CLASS(value);
            printf("<class ");

            if (klass->module != NULL)
                printf("%s.", klass->module->name->chars);

            printf("%s>", klass->name->chars);
            break;
        }

        case OBJ_INSTANCE: {
            jml_obj_class_t *klass = AS_INSTANCE(value)->klass;
            printf("<instance of ");

            if (klass->module != NULL)
                printf("%s.", klass->module->name->chars);

            printf("%s>", klass->name->chars);
            break;
        }

        case OBJ_METHOD:
            jml_obj_function_print(AS_METHOD(value)->method->function);
            break;

        case OBJ_FUNCTION:
            jml_obj_function_print(AS_FUNCTION(value));
            break;

        case OBJ_CLOSURE:
            jml_obj_function_print(AS_CLOSURE(value)->function);
            break;

        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;

        case OBJ_CFUNCTION:
            jml_obj_cfunction_print(AS_CFUNCTION(value));
            break;

        case OBJ_EXCEPTION: {
            jml_obj_exception_t *exc = AS_EXCEPTION(value);
            printf("<exception ");

            if (exc->module != NULL)
                printf("%s.", exc->module->name->chars);

            printf("%s>", exc->name->chars);
            break;
        }
    }
}


static char *
jml_obj_function_stringify(jml_obj_function_t *function)
{
    if (function->name == NULL) {
        return jml_strdup("<fn __main>");
    }

    size_t size = function->name->length * GC_HEAP_GROW_FACTOR;
    char *fn = jml_realloc(NULL, size);

    sprintf(fn, "<fn ");

    if (function->module != NULL) {
        REALLOC(char, fn, size, size + function->module->name->length);
        sprintf(fn, "%s.", function->module->name->chars);
    }

    if (function->klass_name != NULL) {
        REALLOC(char, fn, size, size + function->klass_name->length);
        sprintf(fn, "%s.", function->klass_name->chars);
    }

    REALLOC(char, fn, size, size + function->name->length + 3);
    sprintf(fn, "%s/%d>", function->name->chars,
        function->arity);

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

    sprintf(cfn, "<builtin fn ");

    if (function->module != NULL) {
        REALLOC(char, cfn, size, size + function->module->name->length);
        sprintf(cfn, "%s.", function->module->name->chars);
    }

    if (function->klass_name != NULL) {
        REALLOC(char, cfn, size, size + function->klass_name->length);
        sprintf(cfn, "%s.", function->klass_name->chars);
    }

    REALLOC(char, cfn, size, size + function->name->length + 3);
    sprintf(cfn, "%s>", function->name->chars);

    return cfn;
}


#define CLASS_NAME(buffer, klass)                       \
    if (klass->module != NULL) {                        \
        REALLOC(char, buffer, size,                     \
            size + klass->module->name->length);        \
        sprintf(buffer, "%s.",                          \
            klass->module->name->chars);                \
    }                                                   \
                                                        \
    REALLOC(char, buffer, size,                         \
        size + klass->name->length + 3);                \
    sprintf(buffer, "%s", klass->name->chars)


static char *
jml_obj_class_stringify(jml_obj_class_t *klass)
{
    size_t size = klass->name->length * GC_HEAP_GROW_FACTOR;
    char *buffer = jml_realloc(NULL, size);

    sprintf(buffer, "<class ");

    CLASS_NAME(buffer, klass);

    return buffer;
}


static char *
jml_obj_instance_stringify(jml_obj_instance_t *instance)
{
    size_t size = instance->klass->name->length * GC_HEAP_GROW_FACTOR;
    char *buffer = jml_realloc(NULL, size);

    sprintf(buffer, "<instance of ");

    CLASS_NAME(buffer, instance->klass);

    return buffer;
}


char *
jml_obj_stringify(jml_value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            return jml_strdup(AS_CSTRING(value));

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
                REALLOC(char, buffer, size, size + strlen(temp));
                ptr += sprintf(ptr, "%s, ", temp);
                jml_free(temp);
            }

            char *temp = jml_value_stringify(array.values[item_count]);
            REALLOC(char, buffer, size, size + strlen(temp));
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
                REALLOC(char, buffer, size, size + strlen(temp) + entries[i].key->length + 1);
                ptr += sprintf(ptr, "\"%s\": %s, ", entries[i].key->chars, temp);
                jml_free(temp);
            }

            char *temp = jml_value_stringify(entries[item_count].value);
            REALLOC(char, buffer, size, size + strlen(temp) + entries[item_count].key->length + 2);
            ptr += sprintf(ptr, "\"%s\": %s", entries[item_count].key->chars, temp);
            jml_free(temp);
            jml_free(entries);

            *ptr++ = '}';
            *ptr = 0;

            return buffer;
        }

        case OBJ_MODULE: {
            size_t size = AS_MODULE(value)->name->length + 16;
            char *buffer = jml_alloc(size);

            sprintf(buffer, "<module %s>",
                AS_MODULE(value)->name->chars);
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
                sprintf(buffer, "<exception %s.%s>",
                    exc->module->name->chars,
                    exc->name->chars
                );
            } else {
                sprintf(buffer, "<exception %s>",
                    exc->name->chars);
            }

            return buffer;
        }
    }
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

        case OBJ_CFUNCTION:
            return "<type builtin fn>";

        case OBJ_EXCEPTION:
            return "<type exception>";
    }
    return NULL;
}


bool
jml_obj_is_sentinel(jml_value_t value)
{
    return (AS_OBJ(value) == vm->sentinel);
}


jml_value_t
jml_obj_get_sentinel(void)
{
    return OBJ_VAL(vm->sentinel);
}
