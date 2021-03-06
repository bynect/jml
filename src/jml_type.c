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
        "[MEM] |%p allocate %zd %s|\n",
        (void*)object,
        size,
        jml_obj_stringify_type(OBJ_VAL(object))
    );
#endif
  return object;
}


static jml_obj_string_t *
jml_obj_string_allocate(char *chars, size_t length, uint32_t hash)
{
    jml_obj_string_t *string    = ALLOCATE_OBJ(
        jml_obj_string_t, OBJ_STRING
    );

    string->length              = length;
    string->chars               = chars;
    string->hash                = hash;

    jml_gc_exempt_push(OBJ_VAL(string));
    jml_hashmap_set(&vm->strings, string, NONE_VAL);
    jml_gc_exempt_pop();

    return string;
}


static uint32_t
jml_obj_string_hash(const char *key, size_t length)
{
    uint32_t hash = 2166136261u;

    for (uint32_t i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}


jml_obj_string_t *
jml_obj_string_take(char *chars, size_t length)
{
    uint32_t hash               = jml_obj_string_hash(
        chars, length);

    jml_obj_string_t *interned  = jml_hashmap_find(
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

    if (interned != NULL)
        return interned;

    char *heap_chars            = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length]          = '\0';

    return jml_obj_string_allocate(heap_chars, length, hash);
}


jml_obj_array_t *
jml_obj_array_new(void)
{
    jml_obj_array_t *array      = ALLOCATE_OBJ(
        jml_obj_array_t, OBJ_ARRAY);

    jml_value_array_t value_array;
    jml_value_array_init(&value_array);

    array->values               = value_array;

    return array;
}


void
jml_obj_array_append(jml_obj_array_t *array, jml_value_t value)
{
    jml_gc_exempt_push(value);
    jml_value_array_write(&array->values, value);
    jml_gc_exempt_pop();
}


void
jml_obj_array_add(jml_obj_array_t *source, jml_obj_array_t *dest)
{
    jml_gc_exempt_push(OBJ_VAL(source));
    jml_gc_exempt_push(OBJ_VAL(dest));

    for (int i = 0; i < source->values.count; ++i)
        jml_obj_array_append(dest, source->values.values[i]);

    jml_gc_exempt_pop();
    jml_gc_exempt_pop();
}


jml_obj_map_t *
jml_obj_map_new(void)
{
    jml_obj_map_t *map          = ALLOCATE_OBJ(
        jml_obj_map_t, OBJ_MAP);

    jml_hashmap_t hashmap;
    jml_hashmap_init(&hashmap);

    map->hashmap                = hashmap;

    return map;
}


jml_obj_module_t *
jml_obj_module_new(jml_obj_string_t *name, void *handle)
{
    jml_obj_module_t *module    = ALLOCATE_OBJ(
        jml_obj_module_t, OBJ_MODULE);

    module->name                = name;
    module->handle              = handle;

    jml_hashmap_init(&module->globals);

    return module;
}


jml_obj_class_t *
jml_obj_class_new(jml_obj_string_t *name)
{
    jml_obj_class_t *klass      = ALLOCATE_OBJ(
        jml_obj_class_t, OBJ_CLASS);

    klass->name                 = name;
    klass->super                = NULL;
    klass->inheritable          = true;
    klass->module               = NULL;

    jml_hashmap_init(&klass->statics);

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
    jml_obj_method_t *bound     = ALLOCATE_OBJ(
        jml_obj_method_t, OBJ_METHOD);

    bound->receiver             = receiver;
    bound->method               = method;

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

    jml_obj_closure_t *closure  = ALLOCATE_OBJ(
        jml_obj_closure_t, OBJ_CLOSURE);

    closure->function           = function;
    closure->upvalues           = upvalues;
    closure->upvalue_count      = function->upvalue_count;

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
    function->variadic           = false;
    function->upvalue_count      = 0;
    function->name               = NULL;
    function->klass_name         = NULL;
    function->module             = NULL;

    jml_bytecode_init(&function->bytecode);

    return function;
}


jml_obj_coroutine_t *
jml_obj_coroutine_new(jml_obj_closure_t *closure)
{
    jml_obj_coroutine_t *coro   = ALLOCATE_OBJ(
        jml_obj_coroutine_t, OBJ_COROUTINE);

    coro->stack_capacity        = STACK_MIN;
    coro->stack                 = GROW_ARRAY(jml_value_t, NULL, 0, STACK_MIN);
    coro->stack_top             = coro->stack;

    coro->frame_capacity        = FRAMES_MIN;
    coro->frames                = GROW_ARRAY(jml_call_frame_t, NULL, 0, FRAMES_MIN);
    coro->frame_count           = 0;

    coro->open_upvalues         = NULL;
    coro->caller                = NULL;

    if (closure != NULL) {
        jml_call_frame_t *frame = &coro->frames[coro->frame_count++];
        frame->slots            = coro->stack;
        frame->closure          = closure;
        frame->pc               = closure->function->bytecode.code;

        coro->stack_top[0]      = OBJ_VAL(closure);
        ++coro->stack_top;
    }

    return coro;
}


void
jml_obj_coroutine_grow(jml_obj_coroutine_t *coroutine)
{
    int capacity = GROW_CAPACITY(coroutine->stack_capacity);
    jml_value_t *old_stack = coroutine->stack;

    coroutine->stack = GROW_ARRAY(jml_value_t, coroutine->stack,
        coroutine->stack_capacity, capacity);

    coroutine->stack_capacity = capacity;

    if (coroutine->stack != old_stack) {
        for (uint32_t i = 0; i < coroutine->frame_count; ++i) {
            jml_call_frame_t *frame = &coroutine->frames[i];
            frame->slots = coroutine->stack + (frame->slots - old_stack);
        }

        for (jml_obj_upvalue_t *upvalue = coroutine->open_upvalues;
            upvalue != NULL; upvalue = upvalue->next) {

            upvalue->location = coroutine->stack + (upvalue->location - old_stack);
        }

        coroutine->stack_top = coroutine->stack + (coroutine->stack_top - old_stack);
    }
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
jml_obj_exception_new(const char *name, const char *message)
{
    jml_gc_exempt_push(jml_string_intern(name));
    jml_gc_exempt_push(jml_string_intern(message));

    jml_obj_exception_t *exc    = ALLOCATE_OBJ(
        jml_obj_exception_t, OBJ_EXCEPTION);

    exc->name                   = AS_STRING(jml_gc_exempt_peek(1));
    exc->message                = AS_STRING(jml_gc_exempt_peek(0));
    exc->module                 = NULL;

    jml_gc_exempt_pop();
    jml_gc_exempt_pop();

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
