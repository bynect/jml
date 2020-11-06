#include <stdio.h>
#include <string.h>

#include <jml_common.h>
#include <jml_bytecode.h>
#include <jml_type.h>
#include <jml_value.h>
#include <jml_gc.h>
#include <jml_vm.h>


#define ALLOCATE_OBJ(type, obj_type)                    \
    (type*)jml_obj_allocate(sizeof(type), obj_type)


static jml_obj_t *
jml_obj_allocate(size_t size, jml_obj_type type)
{
    jml_obj_t *object = (jml_obj_t*)jml_reallocate(NULL, 0, size);
    object->type = type;
    object->marked = false;

    object->next = gc->objects;
    gc->objects = object;

#ifdef JML_TRACE_GC
    printf("%p allocate %ld for %d\n", (void*)object, size, type);
#endif
  return object;
}


static jml_obj_string_t *
jml_obj_string_allocate(char *chars,
    size_t length, uint32_t hash)
{
    jml_obj_string_t *string = ALLOCATE_OBJ(jml_obj_string_t, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    jml_vm_push(OBJ_VAL(string));
    jml_hashmap_set(&gc->vm->strings, string, NONE_VAL);
    jml_vm_pop();

    return string;
}


static uint32_t
jml_obj_string_hash(const char *key, size_t length)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
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
        &gc->vm->strings,chars, length, hash
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
    uint32_t hash = jml_obj_string_hash(chars, length);
    jml_obj_string_t *interned = jml_hashmap_find(
        &gc->vm->strings,chars, length, hash
    );

    if (interned != NULL) return interned;

    char *heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return jml_obj_string_allocate(heap_chars, length, hash);
}


jml_obj_class_t *
jml_obj_class_new(jml_obj_string_t *name)
{
    jml_obj_class_t *klass = ALLOCATE_OBJ(jml_obj_class_t, OBJ_CLASS);
    klass->name = name;
    jml_hashmap_init(&klass->methods);
    return klass;
}


jml_obj_instance_t *
jml_obj_instance_new(jml_obj_class_t *klass)
{
    jml_obj_instance_t *instance = ALLOCATE_OBJ(jml_obj_instance_t, OBJ_INSTANCE);
    instance->klass = klass;
    jml_hashmap_init(&instance->fields);
    return instance;
}


jml_obj_method_t *
jml_obj_method_new(jml_value_t receiver,
    jml_obj_closure_t *method)
{
    jml_obj_method_t *bound = ALLOCATE_OBJ(jml_obj_method_t, OBJ_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}


jml_obj_closure_t *
jml_obj_closure_new(jml_obj_function_t *function)
{
    jml_obj_upvalue_t **upvalues = ALLOCATE(jml_obj_upvalue_t*, function->upvalue_count);
    for (int i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    jml_obj_closure_t *closure = ALLOCATE_OBJ(jml_obj_closure_t, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}


jml_obj_upvalue_t *
jml_obj_upvalue_new(jml_value_t *slot)
{
    jml_obj_upvalue_t *upvalue = ALLOCATE_OBJ(jml_obj_upvalue_t, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NONE_VAL;
    upvalue->next = NULL;
    return upvalue;
}


jml_obj_function_t *
jml_obj_function_new(void)
{
    jml_obj_function_t *function = ALLOCATE_OBJ(jml_obj_function_t, OBJ_FUNCTION);

    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    jml_bytecode_init(&function->bytecode);
    return function;
}


jml_obj_cfunction_t *
jml_obj_cfunction_new(jml_cfunction function)
{
    jml_obj_cfunction_t *cfunction = ALLOCATE_OBJ(jml_obj_cfunction_t, OBJ_CFUNCTION);
    cfunction->function = function;
    return cfunction;
}


static void
jml_obj_function_print(jml_obj_function_t *function)
{
    if (function->name == NULL) {
        printf("<fn __main>");
        return;
    }
    printf("<fn %s/%d>", function->name->chars, function->arity);
}


void
jml_obj_print(jml_value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;

        case OBJ_ARRAY:
            printf("<array>");
            break;

        case OBJ_MAP:
            printf("<map>");\
            break;

        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(value)->name->chars);
            break;

        case OBJ_INSTANCE:
            printf("<instance of %s>", AS_INSTANCE(value)->klass->name->chars);
            break;

        case OBJ_METHOD:
            jml_obj_function_print(AS_METHOD(value)->method->function);
            break;

        case OBJ_FUNCTION:
            jml_obj_function_print(AS_FUNCTION(value));
            break;

        case OBJ_CFUNCTION:
            printf("<builtin fn>");
            break;

        case OBJ_CLOSURE:
            jml_obj_function_print(AS_CLOSURE(value)->function);
            break;

        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}
