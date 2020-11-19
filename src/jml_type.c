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
        NULL, 0UL, size);

    object->type                = type;
    object->marked              = false;
    object->next                = vm->objects;
    vm->objects                 = object;

#ifdef JML_TRACE_MEM
    printf(
        "[MEM] |%p allocate %zd (type %s)|\n",
        (void*)object,
        size,
        jml_obj_stringify_type(object)
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
    uint32_t hash = 2166136261U;

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
        &vm->strings,chars, length, hash
    );

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
jml_obj_array_new(jml_value_t *values,
    int item_count)
{
    jml_obj_array_t *array  = ALLOCATE_OBJ(
        jml_obj_array_t, OBJ_ARRAY
    );

    jml_value_array_t value_array;
    jml_value_array_init(&value_array);
    array->values           = value_array;

    for (int i = 0; i < item_count; ++i) {
        jml_value_array_write(&array->values,
        values[i]);
    }

    return array;
}


jml_obj_map_t *
jml_obj_map_new(void)
{
    jml_obj_map_t *map  = ALLOCATE_OBJ(
        jml_obj_map_t, OBJ_MAP
    );

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
    klass->module           = NULL; /*TODO*/

    jml_hashmap_init(&klass->methods);

    return klass;
}


jml_obj_instance_t *
jml_obj_instance_new(jml_obj_class_t *klass)
{
    jml_obj_instance_t *instance    = ALLOCATE_OBJ(
        jml_obj_instance_t, OBJ_INSTANCE);

    instance->klass                 = klass;
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

    jml_obj_closure_t *closure = ALLOCATE_OBJ(
        jml_obj_closure_t, OBJ_CLOSURE);

    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
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

    function->arity              = 0;
    function->upvalue_count      = 0;
    function->name               = NULL;
    
    function->module             = NULL; /*TODO*/

    jml_bytecode_init(&function->bytecode);

    return function;
}


jml_obj_cfunction_t *
jml_obj_cfunction_new(const char *name, 
    jml_cfunction function, jml_obj_module_t *module)
{
    jml_obj_cfunction_t *cfunction = ALLOCATE_OBJ(
        jml_obj_cfunction_t, OBJ_CFUNCTION);

     cfunction->name    = jml_obj_string_copy(
        name, strlen(name));

    cfunction->function = function;

    cfunction->module   = module;

    return cfunction;
}


jml_obj_exception_t *
jml_obj_exception_new(const char *name,
    const char *message)
{
    jml_obj_exception_t *exc = ALLOCATE_OBJ(
        jml_obj_exception_t, OBJ_EXCEPTION);

    exc->name           = jml_obj_string_copy(
        name, strlen(name));

    exc->message        = jml_obj_string_copy(
        message, strlen(message));

    exc->module         = NULL; /*TODO*/

    return exc;
}


jml_obj_exception_t *
jml_obj_exception_format(const char *name,
    char *message_format, ...)
{
    va_list args;
    va_start(args, message_format);

    /*FIXME*/
    size_t size = strlen(message_format) * sizeof(char) * 32;
    char *message = (char*)jml_realloc(NULL, size);
    vsprintf(message, message_format, args);

    va_end(args);

    jml_obj_exception_t *exc = jml_obj_exception_new(
        name, message);
    
    jml_realloc(message, 0UL);

    return exc;
}


jml_obj_module_t *
jml_obj_module_new(const char *name, void *handle)
{
    jml_obj_module_t *module = ALLOCATE_OBJ(
        jml_obj_module_t, OBJ_MODULE);

    module->name    = jml_obj_string_copy(
        name, strlen(name));

    module->handle  = handle;

    jml_hashmap_init(&module->globals);

    return module;
}


static void
jml_obj_function_print(jml_obj_function_t *function)
{
    if (function->name == NULL) {
        printf("<fn __main>");
        return;
    }

    printf("<fn %s/%d>", function->name->chars,
        function->arity);
}


void
jml_obj_print(jml_value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;

        case OBJ_ARRAY: {
            printf("[");
            jml_value_array_t array = AS_ARRAY(value)->values;
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
            jml_hashmap_t hashmap   = AS_MAP(value)->hashmap;
            int item_count          = hashmap.count - 1;

            jml_hashmap_entry_t *entries = jml_hashmap_iterator(&hashmap);

            printf("{");
            for (int i = 0; i < item_count; ++i) {
                printf("\"%s\":", entries[i].key->chars);
                jml_value_print(entries[i].value);
                printf(", ");
            }

            printf("\"%s\":", entries[item_count].key->chars);
            jml_value_print(entries[item_count].value);
            printf("}");

            jml_realloc(entries, 0UL);
            break;
        }

        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(value)->name->chars);
            break;

        case OBJ_MODULE:
            printf("<module %s>", AS_MODULE(value)->name->chars);
            break;

        case OBJ_INSTANCE:
            printf("<instance of %s>", 
                AS_INSTANCE(value)->klass->name->chars);
            break;

        case OBJ_METHOD:
            jml_obj_function_print(
                AS_METHOD(value)->method->function);
            break;

        case OBJ_FUNCTION:
            jml_obj_function_print(AS_FUNCTION(value));
            break;

        case OBJ_CLOSURE:
            jml_obj_function_print(
                AS_CLOSURE(value)->function);
            break;

        case OBJ_UPVALUE:
            printf("upvalue");
            break;

        case OBJ_CFUNCTION:
            printf("<builtin fn>");
            break;

        case OBJ_EXCEPTION:
            printf("<exception>");
            break;
    }
}


static char *
jml_obj_function_stringify(jml_obj_function_t *function)
{
    if (function->name == NULL) {
        return jml_strdup("<fn __main>");
    }

    char fn[11];
    sprintf(fn, "<fn %s/%d>", function->name->chars,
        function->arity);

    return jml_strdup(fn);
}


char *
jml_obj_stringify(jml_value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            return jml_strdup(AS_CSTRING(value));

        case OBJ_ARRAY:
            return jml_strdup("<array>");

        case OBJ_MAP:
            return jml_strdup("<map>");

        case OBJ_CLASS: {
            char cls[11];
            sprintf(cls, "<class %s>", AS_CLASS(value)->name->chars);
            return jml_strdup(cls);
        }

        case OBJ_MODULE: {
            char module[12];
            sprintf(module, "<module %s>",
                AS_MODULE(value)->name->chars);
            return jml_strdup(module);
        }

        case OBJ_INSTANCE: {
            char ins[17];
            sprintf(ins, "<instance of %s>",
                AS_INSTANCE(value)->klass->name->chars);
            return jml_strdup(ins);
        }

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

        case OBJ_CFUNCTION: {
            return jml_strdup("<builtin fn>");
        }

        case OBJ_EXCEPTION: {
            char exc[15];
            sprintf(exc, "<exception %s>",
                AS_EXCEPTION(value)->name->chars);
            return jml_strdup(exc);
        }
    }
    return NULL;
}


char *
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
