#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>

#include <jml.h>

#include <jml/jml_common.h>
#include <jml/jml_vm.h>
#include <jml/jml_gc.h>
#include <jml/jml_compiler.h>
#include <jml/jml_type.h>
#include <jml/jml_module.h>
#include <jml/jml_util.h>


jml_vm_t *vm                        = NULL;


jml_vm_t *
jml_vm_new(void)
{
    jml_vm_t *_vm = jml_alloc(sizeof(jml_vm_t));
    vm = _vm;

    jml_vm_init(_vm);
    return _vm;
}


void
jml_vm_init(jml_vm_t *vm)
{
    vm->objects             = NULL;
    vm->allocated           = 0;
    vm->next_gc             = 1024 * 1024 * 2;
    vm->gray_count          = 0;
    vm->gray_capacity       = 0;
    vm->gray_stack          = NULL;

    vm->running             = NULL;
    vm->current             = NULL;
    vm->external            = NULL;

    vm->compilers[0]        = NULL;
    vm->compiler_top        = vm->compilers;

    vm->exempt_top          = vm->exempt_stack;

    jml_hashmap_init(&vm->globals);
    jml_hashmap_init(&vm->strings);
    jml_hashmap_init(&vm->modules);
    jml_hashmap_init(&vm->builtins);

    vm->main_string         = NULL;
    vm->module_string       = NULL;
    vm->path_string         = NULL;
    vm->init_string         = NULL;
    vm->call_string         = NULL;
    vm->free_string         = NULL;
    vm->add_string          = NULL;
    vm->sub_string          = NULL;
    vm->mul_string          = NULL;
    vm->pow_string          = NULL;
    vm->div_string          = NULL;
    vm->mod_string          = NULL;
    vm->gt_string           = NULL;
    vm->ge_string           = NULL;
    vm->lt_string           = NULL;
    vm->le_string           = NULL;
    vm->concat_string       = NULL;
    vm->get_string          = NULL;
    vm->set_string          = NULL;
    vm->size_string         = NULL;
    vm->print_string        = NULL;
    vm->str_string          = NULL;

    vm->main_string         = jml_obj_string_copy("__main", 6);
    vm->module_string       = jml_obj_string_copy("__module", 8);
    vm->path_string         = jml_obj_string_copy("__path", 6);
    vm->init_string         = jml_obj_string_copy("__init", 6);
    vm->call_string         = jml_obj_string_copy("__call", 6);
    vm->free_string         = jml_obj_string_copy("__free", 6);
    vm->add_string          = jml_obj_string_copy("__add", 5);
    vm->sub_string          = jml_obj_string_copy("__sub", 5);
    vm->mul_string          = jml_obj_string_copy("__mul", 5);
    vm->pow_string          = jml_obj_string_copy("__pow", 5);
    vm->div_string          = jml_obj_string_copy("__div", 5);
    vm->mod_string          = jml_obj_string_copy("__mod", 5);
    vm->gt_string           = jml_obj_string_copy("__gt", 4);
    vm->ge_string           = jml_obj_string_copy("__ge", 4);
    vm->lt_string           = jml_obj_string_copy("__lt", 4);
    vm->le_string           = jml_obj_string_copy("__le", 4);
    vm->concat_string       = jml_obj_string_copy("__concat", 8);
    vm->get_string          = jml_obj_string_copy("__get", 5);
    vm->set_string          = jml_obj_string_copy("__set", 5);
    vm->size_string         = jml_obj_string_copy("__size", 6);
    vm->print_string        = jml_obj_string_copy("__print", 7);
    vm->str_string          = jml_obj_string_copy("__str", 5);

    jml_core_register(vm);
}


void
jml_vm_free(jml_vm_t *vm)
{
    jml_hashmap_free(&vm->globals);
    jml_hashmap_free(&vm->strings);
    jml_hashmap_free(&vm->modules);
    jml_hashmap_free(&vm->builtins);

    vm->main_string         = NULL;
    vm->module_string       = NULL;
    vm->path_string         = NULL;
    vm->init_string         = NULL;
    vm->call_string         = NULL;
    /*vm->free_string         = NULL;*/
    vm->add_string          = NULL;
    vm->sub_string          = NULL;
    vm->mul_string          = NULL;
    vm->pow_string          = NULL;
    vm->div_string          = NULL;
    vm->mod_string          = NULL;
    vm->gt_string           = NULL;
    vm->ge_string           = NULL;
    vm->lt_string           = NULL;
    vm->le_string           = NULL;
    vm->concat_string       = NULL;
    vm->get_string          = NULL;
    vm->set_string          = NULL;
    vm->size_string         = NULL;
    vm->print_string        = NULL;
    vm->str_string          = NULL;

    vm->running             = NULL;
    vm->current             = NULL;
    vm->external            = NULL;

    jml_gc_free_objs();

    JML_ASSERT(
        vm->allocated == 0,
        "%zu bytes not freed\n",
        vm->allocated
    );

    jml_free(vm);
}


static inline void
jml_vm_push(jml_value_t value)
{
    if ((vm->running->stack_top + 1 - vm->running->stack)
        >= vm->running->stack_capacity) {

        jml_obj_coroutine_grow(vm->running);
    }

    *vm->running->stack_top++ = value;
}


static inline jml_value_t
jml_vm_pop(void)
{
    --vm->running->stack_top;
    return *vm->running->stack_top;
}


static inline jml_value_t
jml_vm_pop_two(void)
{
    vm->running->stack_top -= 2;
    return *vm->running->stack_top;
}


static inline void
jml_vm_rot(void)
{
    jml_value_t a       = jml_vm_pop();
    jml_value_t b       = jml_vm_pop();
    jml_vm_push(a);
    jml_vm_push(b);
}


static inline jml_value_t
jml_vm_peek(int distance)
{
    return vm->running->stack_top[-1 - distance];
}


static inline bool
jml_vm_global_get(jml_value_t module, jml_obj_string_t *name,
    jml_value_t **value)
{
    if (IS_NONE(module))
        return jml_hashmap_get(&vm->globals, name, value);

    JML_ASSERT(IS_MODULE(module), "");
    if (jml_hashmap_get(&vm->builtins, name, value))
        return true;

    return jml_hashmap_get(&AS_MODULE(module)->globals, name, value);
}


static inline bool
jml_vm_global_set(jml_value_t module, jml_obj_string_t *name,
    jml_value_t value)
{
    if (IS_NONE(module))
        return jml_hashmap_set(&vm->globals, name, value);

    JML_ASSERT(IS_MODULE(module), "");
    return jml_hashmap_set(&AS_MODULE(module)->globals, name, value);
}


static inline bool
jml_vm_global_del(jml_value_t module, jml_obj_string_t *name)
{
    if (IS_NONE(module))
        return jml_hashmap_del(&vm->globals, name);

    JML_ASSERT(IS_MODULE(module), "");
    return jml_hashmap_del(&AS_MODULE(module)->globals, name);
}


static inline bool
jml_vm_global_pop(jml_value_t module, jml_obj_string_t *name,
    jml_value_t *value)
{
    if (IS_NONE(module))
        return jml_hashmap_pop(&vm->globals, name, value);

    JML_ASSERT(IS_MODULE(module), "");
    return jml_hashmap_pop(&AS_MODULE(module)->globals, name, value);
}


JML_FORMAT(1, 2) void
jml_vm_error(const char *format, ...)
{
    if (vm->running == NULL)
        return;

#ifndef JML_BACKTRACE
    if (vm->external != NULL) {
        jml_call_frame_t    *frame    = &vm->running->frames[0];
        jml_obj_function_t  *function = frame->closure->function;

        size_t instruction = frame->pc - function->bytecode.code - 1;

        fprintf(stderr, "[line %d] in function ",
            function->bytecode.lines[instruction]
        );

        if (vm->external->module != NULL)
            fprintf(stderr, "%.*s.", (int32_t)vm->external->module->name->length,
                vm->external->module->name->chars);

        if (vm->external->klass_name != NULL)
            fprintf(stderr, "%.*s.", (int32_t)vm->external->klass_name->length,
                vm->external->klass_name->chars);

        fprintf(stderr, "%.*s\n", (int32_t)vm->external->name->length,
            vm->external->name->chars);
    }

    for (int32_t i = vm->running->frame_count - 1; i >= 0; --i) {
#else
    for (uint32_t i = 0; i < vm->running->frame_count; ++i) {
#endif
        jml_call_frame_t    *frame    = &vm->running->frames[i];
        jml_obj_function_t  *function = frame->closure->function;

        size_t instruction = frame->pc - function->bytecode.code - 1;

        fprintf(stderr, "[line %d] in function ",
            function->bytecode.lines[instruction]
        );

        if (function->name != NULL) {
            if (function->module != NULL) {
                fprintf(stderr, "%.*s.", (int32_t)function->module->name->length,
                    function->module->name->chars);
            }

            if (function->klass_name != NULL) {
                fprintf(stderr, "%.*s.", (int32_t)function->klass_name->length,
                    function->klass_name->chars);
            }

            fprintf(stderr, "%.*s/%d\n", (int32_t)function->name->length,
                function->name->chars, function->arity);
        } else
            fprintf(stderr, "__main\n");
    }

#ifdef JML_BACKTRACE
    if (vm->external != NULL) {
        jml_call_frame_t    *frame    = &vm->running->frames[vm->running->frame_count - 1];
        jml_obj_function_t  *function = frame->closure->function;

        size_t instruction = frame->pc - function->bytecode.code - 1;

        fprintf(stderr, "[line %d] in function ",
            function->bytecode.lines[instruction]
        );

        if (vm->external->module != NULL)
            fprintf(stderr, "%.*s.", (int32_t)vm->external->module->name->length,
                vm->external->module->name->chars);

        if (vm->external->klass_name != NULL)
            fprintf(stderr, "%.*s.", (int32_t)vm->external->klass_name->length,
                vm->external->klass_name->chars);

        fprintf(stderr, "%.*s\n", (int32_t)vm->external->name->length,
            vm->external->name->chars);
    }
#endif

    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);

    vm->external                = NULL;
    vm->running                 = NULL;
}


/*forwarded declaration*/
static void jml_vm_upvalue_close(
    jml_obj_coroutine_t *coroutine, jml_value_t *last);


static bool
jml_vm_exception(jml_obj_exception_t *exc)
{
    for (int32_t i = vm->running->frame_count; i > 0; --i) {
        jml_call_frame_t *frame      = &vm->running->frames[i - 1];

        if (frame->pc[-3] == OP_TRY_CALL
            || frame->pc[-4] == OP_TRY_INVOKE
            || frame->pc[-6] == EXTENDED_OP(OP_TRY_INVOKE)
            || frame->pc[-4] == OP_TRY_SUPER_INVOKE
            || frame->pc[-6] == EXTENDED_OP(OP_TRY_SUPER_INVOKE)) {

            if (vm->external != NULL) {
                vm->external         = NULL;
                jml_vm_push(OBJ_VAL(exc));
                return true;
            }

            int locals = frame->pc[-1] + 1;
            printf("%d\n\n", locals);
            jml_vm_upvalue_close(vm->running, frame->slots + locals);
            vm->running->frame_count = i;
            vm->running->stack_top   = frame->slots + locals;

            jml_vm_push(OBJ_VAL(exc));
            jml_vm_rot();
            return true;
        }
    }

    jml_vm_error(
        "%.*s: %.*s",
        (int32_t)exc->name->length, exc->name->chars,
        (int32_t)exc->message->length, exc->message->chars
    );

    return false;
}


static bool
jml_vm_call(jml_obj_coroutine_t *coroutine,
    jml_obj_closure_t *closure, int arg_count)
{
    if (closure->function->variadic) {
        if (arg_count < closure->function->arity - 1) {
            jml_vm_error(
                "TooFewArgs: Expected '%d' arguments but got '%d'.",
                closure->function->arity - 1, arg_count
            );
            return false;
        }

        int             item_count  = arg_count - closure->function->arity + 1;
        jml_value_t     *values     = coroutine->stack_top -= item_count;
        jml_obj_array_t *array      = jml_obj_array_new();
        jml_gc_exempt_push(OBJ_VAL(array));

        for (int i = 0; i < item_count; ++i) {
            jml_obj_array_append(array, values[i]);
        }

        jml_gc_exempt_pop();
        jml_vm_push(OBJ_VAL(array));

    } else {
        if (arg_count < closure->function->arity) {
            jml_vm_error(
                "TooFewArgs: Expected '%d' arguments but got '%d'.",
                closure->function->arity, arg_count
            );
            return false;
        }

        if (arg_count > closure->function->arity) {
            jml_vm_error(
                "TooManyArgs: Expected '%d' arguments but got '%d'.",
                closure->function->arity, arg_count
            );
            return false;
        }
    }

    if (coroutine->frame_count == FRAMES_MAX) {
        jml_vm_error("OverflowErr: Scope depth overflow.");
        return false;
    }

    if (coroutine->frame_count + 1 >= coroutine->frame_capacity)
        jml_obj_coroutine_grow(coroutine);

    jml_call_frame_t *frame = &coroutine->frames[coroutine->frame_count++];
    frame->closure = closure;
    frame->pc = closure->function->bytecode.code;
    frame->slots = closure->function->variadic
        ? coroutine->stack_top - closure->function->arity - 1
        : coroutine->stack_top - arg_count - 1;

    return true;
}


bool
jml_vm_call_value(jml_obj_coroutine_t *coroutine,
    jml_value_t callee, int arg_count)
{
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_METHOD: {
                jml_obj_method_t *bound         = AS_METHOD(callee);
                coroutine->stack_top[-arg_count - 1]   = bound->receiver;

                return jml_vm_call(coroutine, bound->method, arg_count);
            }

            case OBJ_CLOSURE: {
                return jml_vm_call(coroutine, AS_CLOSURE(callee), arg_count);
            }

            case OBJ_CLASS: {
                jml_obj_class_t    *klass       = AS_CLASS(callee);
                jml_value_t         instance    = OBJ_VAL(
                    jml_obj_instance_new(klass));

                coroutine->stack_top[-arg_count - 1]   = instance;
                jml_value_t *initializer;

                if (jml_hashmap_get(&klass->statics, vm->init_string, &initializer)) {
                    if (IS_CFUNCTION(*initializer)) {
                        jml_vm_push(instance);
                        ++arg_count;

                        jml_obj_cfunction_t *cfunction_obj  = AS_CFUNCTION(*initializer);
                        jml_cfunction        cfunction      = cfunction_obj->function;
                        jml_value_t          result         = cfunction(
                            arg_count, coroutine->stack_top - arg_count);

                        coroutine->stack_top                -= arg_count + 1;

                        if (IS_EXCEPTION(result)) {
                            jml_obj_exception_t *exception  = AS_EXCEPTION(result);
                            exception->module               = cfunction_obj->module;
                            vm->external                    = cfunction_obj;
                            return jml_vm_exception(exception);

                        } else {
                            jml_vm_push(instance);
                            return true;
                        }

                    } else
                        return jml_vm_call(coroutine, AS_CLOSURE(*initializer), arg_count);

                } else if (arg_count != 0) {
                    jml_vm_error(
                        "TooManyArgs: Expected '0' arguments but got '%d'.", arg_count
                    );
                    return false;
                }
                return true;
            }

            case OBJ_INSTANCE: {
                jml_obj_instance_t *instance    = AS_INSTANCE(callee);
                jml_value_t        *caller;

                if (jml_hashmap_get(&instance->klass->statics, vm->call_string, &caller)) {
                    if (IS_CFUNCTION(*caller)) {
                        jml_vm_push(callee);
                        return jml_vm_call_value(coroutine, *caller, arg_count + 1);
                    }
                    return jml_vm_call(coroutine, AS_CLOSURE(*caller), arg_count);

                } else {
                    jml_vm_error(
                        "DiffTypes: Can't call instance of '%.*s'.",
                        (int32_t)instance->klass->name->length,
                        instance->klass->name->chars
                    );
                    return false;
                }
            }

            case OBJ_CFUNCTION: {
                jml_obj_cfunction_t *cfunction_obj  = AS_CFUNCTION(callee);
                jml_cfunction        cfunction      = cfunction_obj->function;
                jml_value_t          result         = cfunction(
                    arg_count, coroutine->stack_top - arg_count);

                coroutine->stack_top                -= arg_count + 1;

                if (IS_EXCEPTION(result)) {
                    jml_obj_exception_t *exception  = AS_EXCEPTION(result);
                    exception->module               = cfunction_obj->module;
                    vm->external                    = cfunction_obj;
                    return jml_vm_exception(exception);

                } else {
                    jml_vm_push(result);
                    return true;
                }
            }

            default:
                break;
        }
    }

    jml_vm_error(
        "DiffTypes: Can call value of type %s.",
        jml_obj_stringify_type(callee)
    );
    return false;
}


static bool
jml_vm_invoke_class(jml_obj_coroutine_t *coroutine,
    jml_obj_class_t *klass, jml_obj_string_t *name, int arg_count)
{
    jml_value_t *value;

    if (!jml_hashmap_get(&klass->statics, name, &value))
        return false;

    if (IS_CLOSURE(*value))
        return jml_vm_call(coroutine, AS_CLOSURE(*value), arg_count);

    else if (IS_CFUNCTION(*value)) {
        jml_vm_push(OBJ_VAL(jml_vm_peek(arg_count)));
        return jml_vm_call_value(coroutine, *value, arg_count + 1);

    } else
        return jml_vm_call_value(coroutine, *value, arg_count);
}


static bool
jml_vm_invoke_instance(jml_obj_coroutine_t *coroutine,
    jml_obj_instance_t *instance, jml_obj_string_t *name, int arg_count)
{
    jml_value_t *value;

    if (!jml_hashmap_get(&instance->klass->statics, name, &value))
        return false;

    if (IS_CLOSURE(*value))
        return jml_vm_call(coroutine, AS_CLOSURE(*value), arg_count);

    else if (IS_CFUNCTION(*value)) {
        jml_vm_push(OBJ_VAL(instance));
        return jml_vm_call_value(coroutine, *value, arg_count + 1);

    } else
        return jml_vm_call_value(coroutine, *value, arg_count);
}


static bool
jml_vm_invoke(jml_obj_coroutine_t *coroutine,
    jml_obj_string_t *name, int arg_count)
{
    jml_value_t receiver = jml_vm_peek(arg_count);

    if (IS_INSTANCE(receiver)) {
        jml_obj_instance_t *instance = AS_INSTANCE(receiver);
        jml_value_t        *value;

        if (jml_hashmap_get(&instance->fields, name, &value)) {
            coroutine->stack_top[-arg_count - 1] = *value;
            return jml_vm_call_value(coroutine, *value, arg_count);
        }

        if (!jml_vm_invoke_instance(coroutine, instance, name, arg_count)) {
            jml_vm_error(
                "UndefErr: Undefined property '%.*s'.",
                (int32_t)name->length, name->chars
            );
            return false;
        }
        return true;

    } else if (IS_CLASS(receiver)) {
        jml_value_t *value;

        if (!jml_hashmap_get(&AS_CLASS(receiver)->statics, name, &value))
            return false;

        if (!IS_CLOSURE(*value)
            && !IS_CFUNCTION(*value)
            && !IS_METHOD(*value))
            return jml_vm_call_value(coroutine, *value, arg_count);

        jml_vm_error(
            "DiffTypes: Can call only instance methods."
        );
        return false;

    } else if (IS_MODULE(receiver)) {
        jml_obj_module_t *module    = AS_MODULE(receiver);
        jml_value_t      *value;

        if (jml_hashmap_get(&module->globals, name, &value)) {
            coroutine->stack_top[-arg_count - 1] = *value;
            return jml_vm_call_value(coroutine, *value, arg_count);
        }

#ifndef JML_LAZY_IMPORT
        else {
            jml_vm_error(
                "UndefErr: Undefined property '%.*s'.",
                (int32_t)name->length, name->chars
            );
            return false;
        }
#else
        else if (module->handle != NULL) {
            jml_obj_cfunction_t *cfunction = jml_module_get_raw(
                module, name->chars, true);

            if (cfunction == NULL || cfunction->function == NULL) {
                jml_vm_error(
                    "UndefErr: Undefined property '%.*s'.",
                    name->length, name->chars
                );
                return false;
            }

            jml_vm_push(OBJ_VAL(cfunction));
            jml_hashmap_set(&module->globals, name, jml_vm_peek(0));
            jml_vm_pop();
            return jml_vm_call_value(OBJ_VAL(cfunction), arg_count);
        }
#endif
    }

    jml_vm_error(
        "DiffTypes: Can't call '%.*s'.",
        (int32_t)name->length, name->chars
    );
    return false;
}


static bool
jml_vm_class_field_bind(jml_obj_class_t *klass, jml_obj_string_t *name)
{
    jml_value_t *value;

    if (!jml_hashmap_get(&klass->statics, name, &value)) {
        jml_vm_error(
            "UndefErr: Undefined property '%.*s'.",
            (int32_t)name->length, name->chars
        );
        return false;
    }

    jml_value_t field;
    if (IS_CFUNCTION(*value)) {
        field = OBJ_VAL(AS_CFUNCTION(*value));

    } else if (IS_CLOSURE(*value)) {
        field = OBJ_VAL(jml_obj_method_new(
            jml_vm_peek(0), AS_CLOSURE(*value)
        ));

    } else
        field = *value;

    jml_vm_pop();
    jml_vm_push(field);
    return true;
}


static void
jml_vm_class_field_define(jml_obj_string_t *name)
{
    jml_value_t      value          = jml_vm_peek(0);
    jml_obj_class_t *klass          = AS_CLASS(jml_vm_peek(1));

    jml_hashmap_set(&klass->statics, name, value);
    jml_vm_pop();
}


static jml_obj_upvalue_t *
jml_vm_upvalue_capture(jml_obj_coroutine_t *coroutine, jml_value_t *local)
{
    jml_obj_upvalue_t *previous     = NULL;
    jml_obj_upvalue_t *upvalue      = coroutine->open_upvalues;

    while (upvalue != NULL
        && upvalue->location > local) {

        previous = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local)
        return upvalue;

    jml_obj_upvalue_t *new_upvalue  = jml_obj_upvalue_new(local);
    new_upvalue->next               = upvalue;

    if (previous == NULL)
        coroutine->open_upvalues    = new_upvalue;
    else
        previous->next              = new_upvalue;

    return new_upvalue;
}


static void
jml_vm_upvalue_close(jml_obj_coroutine_t *coroutine, jml_value_t *last)
{
    while (coroutine->open_upvalues != NULL
        && coroutine->open_upvalues->location >= last) {

        jml_obj_upvalue_t *upvalue  = coroutine->open_upvalues;
        upvalue->closed             = *upvalue->location;
        upvalue->location           = &upvalue->closed;
        coroutine->open_upvalues    = upvalue->next;
    }
}


static void
jml_string_concatenate(void)
{
    jml_obj_string_t *b = AS_STRING(jml_vm_peek(0));
    jml_obj_string_t *a = AS_STRING(jml_vm_peek(1));

    size_t length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);

    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    jml_obj_string_t *result = jml_obj_string_take(chars, length);
    jml_vm_pop_two();
    jml_vm_push(OBJ_VAL(result));
}


static jml_obj_array_t *
jml_array_copy(jml_obj_array_t *array)
{
    jml_vm_push(OBJ_VAL(jml_obj_array_new()));

    for (int i = 0; i < array->values.count; ++i) {
        jml_obj_array_append(AS_ARRAY(jml_vm_peek(0)),
            array->values.values[i]);
    }

    return AS_ARRAY(jml_vm_pop());
}


static void
jml_array_concatenate(void)
{
    jml_obj_array_t        *array   = AS_ARRAY(jml_vm_peek(1));
    jml_value_t             value   = jml_vm_peek(0);

    jml_obj_array_t        *copy    = jml_array_copy(array);

    if (IS_ARRAY(value)) {
        jml_obj_array_t    *array2  = AS_ARRAY(value);

        if (array == array2) goto end;

        for (int i = 0; i < array2->values.count; ++i)
            jml_value_array_write(&copy->values,
                array2->values.values[i]);
    } else {
        jml_value_array_write(&copy->values, value);
        goto end;
    }

    end: {
        jml_vm_pop_two();
        jml_vm_push(OBJ_VAL(copy));
    }
}


static bool
jml_vm_module_import(jml_obj_string_t *fullname,
    jml_obj_string_t *name)
{
    jml_value_t *value;

    if (!jml_hashmap_get(&vm->modules, fullname, &value)) {
        jml_value_t path_value;

        jml_obj_module_t *module = jml_module_open(fullname, name, &path_value);

        if (module == NULL)
            return false;

        jml_vm_push(OBJ_VAL(module));
        jml_vm_push(OBJ_VAL(path_value));

        jml_hashmap_set(&vm->modules, fullname, jml_vm_peek(1));
        jml_hashmap_set(&module->globals, vm->path_string, jml_vm_peek(0));
        jml_hashmap_set(&module->globals, vm->module_string, OBJ_VAL(fullname));

        jml_vm_pop();

        if (!jml_module_initialize(module)) {
            jml_vm_error(
                "ImportErr: Importing module '%.*s' failed.",
                (int32_t)module->name->length,
                module->name->chars
            );
            return false;
        }

    } else
        jml_vm_push(*value);

    return true;
}


static bool
jml_vm_module_bind(jml_obj_module_t *module,
    jml_obj_string_t *name)
{
    jml_value_t  function;
    jml_value_t *value;

    if (!jml_hashmap_get(&module->globals, name, &value)) {
#ifdef JML_LAZY_IMPORT
        jml_obj_cfunction_t *cfunction = jml_module_get_raw(
            module, name->chars, true);

        if (cfunction != NULL) {
            function = OBJ_VAL(cfunction);
            jml_vm_push(function);

            jml_hashmap_set(&module->globals,
                name, jml_vm_peek(0));

            jml_vm_pop();
        }
#else
        goto err;
#endif
        err: {
            jml_vm_error(
                "UndefErr: Undefined member '%.*s'.",
                (int32_t)name->length, name->chars
            );
            return false;
        }
    } else
        function = *value;

    jml_vm_pop();
    jml_vm_push(function);
    return true;
}


static jml_interpret_result
jml_vm_run(jml_value_t *last)
{
    register jml_obj_coroutine_t *running   = vm->running;
    register jml_call_frame_t *frame        = &running->frames[running->frame_count - 1];
    register uint8_t *pc                    = frame->pc;

#ifndef JML_EVAL
    (void) last;
#endif


#define SAVE_FRAME()                (frame->pc = pc)
#define LOAD_FRAME()                                    \
    do {                                                \
        running = vm->running;                          \
        frame   = &running->frames[                     \
            running->frame_count - 1                    \
        ];                                              \
        pc      = frame->pc;                            \
    } while (false)


#define RUNTIME_ERROR(fmt, ...)                         \
    do {                                                \
        vm->running = running;                          \
        jml_vm_error(fmt, ## __VA_ARGS__);              \
    } while (false)


#define READ_BYTE()                 (*pc++)
#define READ_SHORT()                (pc += 2, (uint16_t)((pc[-2] << 8) | pc[-1]))
#define READ_LONG()                                     \
    (pc += 4, (uint32_t)((pc[-4] << 24) | (pc[-3] << 16) | (pc[-2] << 8) | pc[-1]))

#define READ_STRING()               AS_STRING(READ_CONST())
#define READ_CSTRING()              AS_CSTRING(READ_CONST())
#define READ_CONST()                                    \
    (frame->closure->function->bytecode.constants.values[READ_BYTE()])

#define READ_STRING_EXTENDED()      AS_STRING(READ_CONST_EXTENDED())
#define READ_CSTRING_EXTENDED()     AS_CSTRING(READ_CONST_EXTENDED())
#define READ_CONST_EXTENDED()                           \
    (frame->closure->function->bytecode.constants.values[READ_SHORT()])


#define BINARY_OP(type, op, num_type, verb, string)     \
    do {                                                \
        jml_value_t a = jml_vm_peek(1);                 \
        jml_value_t b = jml_vm_peek(0);                 \
                                                        \
        if (IS_NUM(a) && IS_NUM(b)) {                   \
            jml_vm_pop_two();                           \
            jml_vm_push(type(                           \
                (num_type)AS_NUM(a)                     \
                op                                      \
                (num_type)AS_NUM(b)                     \
            ));                                         \
                                                        \
        } else if (IS_INSTANCE(a)) {                    \
            SAVE_FRAME();                               \
            jml_obj_instance_t *obj = AS_INSTANCE(a);   \
            if (!jml_vm_invoke_instance(                \
                running, obj, string, 1)) {             \
                RUNTIME_ERROR(                          \
                    "DiffTypes: Can't " verb            \
                    " instance of '%.*s'.",             \
                    (int32_t)obj->klass->name->length,  \
                    obj->klass->name->chars             \
                );                                      \
                return INTERPRET_RUNTIME_ERROR;         \
            }                                           \
            LOAD_FRAME();                               \
            END_OP();                                   \
                                                        \
        } else {                                        \
            SAVE_FRAME();                               \
            RUNTIME_ERROR(                              \
                "DiffTypes: "                           \
                "Operands must be numbers or instances."\
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
    } while (false)


#define BINARY_DIV(type, op, num_type, verb, string)    \
    do {                                                \
        jml_value_t a = jml_vm_peek(1);                 \
        jml_value_t b = jml_vm_peek(0);                 \
                                                        \
        if (IS_NUM(a) && IS_NUM(b)) {                   \
            jml_vm_pop_two();                           \
            if (AS_NUM(b) == 0) {                       \
                SAVE_FRAME();                           \
                RUNTIME_ERROR(                          \
                    "DivByZero: Can't divide by zero."  \
                );                                      \
                return INTERPRET_RUNTIME_ERROR;         \
            }                                           \
            jml_vm_push(type(                           \
                (num_type)AS_NUM(a)                     \
                op                                      \
                (num_type)AS_NUM(b)                     \
            ));                                         \
                                                        \
        } else if (IS_INSTANCE(a)) {                    \
            SAVE_FRAME();                               \
            jml_obj_instance_t *obj = AS_INSTANCE(a);   \
            if (!jml_vm_invoke_instance(                \
                running, obj, string, 1)) {             \
                RUNTIME_ERROR(                          \
                    "DiffTypes: Can't " verb            \
                    " instance of '%.*s'.",             \
                    (int32_t)obj->klass->name->length,  \
                    obj->klass->name->chars             \
                );                                      \
                return INTERPRET_RUNTIME_ERROR;         \
            }                                           \
            LOAD_FRAME();                               \
            END_OP();                                   \
                                                        \
        } else {                                        \
            SAVE_FRAME();                               \
            RUNTIME_ERROR(                              \
                "DiffTypes: "                           \
                "Operands must be numbers or instances."\
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
    } while (false)


#define BINARY_FN(type, fn, num_type, verb, string)     \
    do {                                                \
        jml_value_t a = jml_vm_peek(1);                 \
        jml_value_t b = jml_vm_peek(0);                 \
                                                        \
        if (IS_NUM(a) && IS_NUM(b)) {                   \
            jml_vm_pop_two();                           \
            jml_vm_push(type(fn(                        \
                (num_type)AS_NUM(a),                    \
                (num_type)AS_NUM(b)                     \
            )));                                        \
                                                        \
        } else if (IS_INSTANCE(a)) {                    \
            SAVE_FRAME();                               \
            jml_obj_instance_t *obj = AS_INSTANCE(a);   \
            if (!jml_vm_invoke_instance(                \
                running, obj, string, 1)) {             \
                RUNTIME_ERROR(                          \
                    "DiffTypes: Can't " verb            \
                    " instance of '%.*s'.",             \
                    (int32_t)obj->klass->name->length,  \
                    obj->klass->name->chars             \
                );                                      \
                return INTERPRET_RUNTIME_ERROR;         \
            }                                           \
            LOAD_FRAME();                               \
            END_OP();                                   \
                                                        \
        } else {                                        \
            SAVE_FRAME();                               \
            RUNTIME_ERROR(                              \
                "DiffTypes: "                           \
                "Operands must be numbers or instances."\
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
    } while (false)


#ifdef JML_COMPUTED_GOTO

#define EXEC_OP_(op)                exec_ ## op
#define TABLE_OP(op)                &&EXEC_OP_(op)

#define DISPATCH()                  goto *dispatcher[READ_BYTE()]
#define EXEC_OP(op)                 EXEC_OP_(op):


#ifdef JML_TRACE_STACK

#define END_OP_()                   goto trace_stack

#else

#define END_OP_()                   DISPATCH()

#endif


#ifdef JML_ASSERTION

#define END_OP()                                        \
    do {                                                \
        JML_ASSERT(                                     \
            *pc >= OP_NOP && *pc <= OP_END,             \
            "Out of range op (%u)", *pc                 \
        );                                              \
        END_OP_();                                      \
    } while (false)

#else

#define END_OP()                    END_OP_()

#endif

    static const void *dispatcher[] = {
        TABLE_OP(OP_NOP),
        TABLE_OP(OP_POP),
        TABLE_OP(OP_POP_TWO),
        TABLE_OP(OP_ROT),
        TABLE_OP(OP_SAVE),
        TABLE_OP(OP_CONST),
        TABLE_OP(EXTENDED_OP(OP_CONST)),
        TABLE_OP(OP_NONE),
        TABLE_OP(OP_TRUE),
        TABLE_OP(OP_FALSE),
        TABLE_OP(OP_BOOL),
        TABLE_OP(OP_ADD),
        TABLE_OP(OP_SUB),
        TABLE_OP(OP_MUL),
        TABLE_OP(OP_POW),
        TABLE_OP(OP_DIV),
        TABLE_OP(OP_MOD),
        TABLE_OP(OP_NOT),
        TABLE_OP(OP_NEG),
        TABLE_OP(OP_EQUAL),
        TABLE_OP(OP_GREATER),
        TABLE_OP(OP_GREATEREQ),
        TABLE_OP(OP_LESS),
        TABLE_OP(OP_LESSEQ),
        TABLE_OP(OP_NOTEQ),
        TABLE_OP(OP_CONCAT),
        TABLE_OP(OP_CONTAIN),
        TABLE_OP(OP_JUMP),
        TABLE_OP(OP_JUMP_IF_FALSE),
        TABLE_OP(OP_LOOP),
        TABLE_OP(OP_CALL),
        TABLE_OP(OP_TRY_CALL),
        TABLE_OP(OP_INVOKE),
        TABLE_OP(EXTENDED_OP(OP_INVOKE)),
        TABLE_OP(OP_TRY_INVOKE),
        TABLE_OP(EXTENDED_OP(OP_TRY_INVOKE)),
        TABLE_OP(OP_SUPER_INVOKE),
        TABLE_OP(EXTENDED_OP(OP_SUPER_INVOKE)),
        TABLE_OP(OP_TRY_SUPER_INVOKE),
        TABLE_OP(EXTENDED_OP(OP_TRY_SUPER_INVOKE)),
        TABLE_OP(OP_CLOSURE),
        TABLE_OP(EXTENDED_OP(OP_CLOSURE)),
        TABLE_OP(OP_RETURN),
        TABLE_OP(OP_SPREAD),
        TABLE_OP(OP_CLASS),
        TABLE_OP(EXTENDED_OP(OP_CLASS)),
        TABLE_OP(OP_CLASS_FIELD),
        TABLE_OP(EXTENDED_OP(OP_CLASS_FIELD)),
        TABLE_OP(OP_INHERIT),
        TABLE_OP(OP_SET_LOCAL),
        TABLE_OP(EXTENDED_OP(OP_SET_LOCAL)),
        TABLE_OP(OP_GET_LOCAL),
        TABLE_OP(EXTENDED_OP(OP_GET_LOCAL)),
        TABLE_OP(OP_SET_UPVALUE),
        TABLE_OP(EXTENDED_OP(OP_SET_UPVALUE)),
        TABLE_OP(OP_GET_UPVALUE),
        TABLE_OP(EXTENDED_OP(OP_GET_UPVALUE)),
        TABLE_OP(OP_CLOSE_UPVALUE),
        TABLE_OP(OP_SET_GLOBAL),
        TABLE_OP(EXTENDED_OP(OP_SET_GLOBAL)),
        TABLE_OP(OP_GET_GLOBAL),
        TABLE_OP(EXTENDED_OP(OP_GET_GLOBAL)),
        TABLE_OP(OP_DEF_GLOBAL),
        TABLE_OP(EXTENDED_OP(OP_DEF_GLOBAL)),
        TABLE_OP(OP_DEL_GLOBAL),
        TABLE_OP(EXTENDED_OP(OP_DEL_GLOBAL)),
        TABLE_OP(OP_SET_MEMBER),
        TABLE_OP(EXTENDED_OP(OP_SET_MEMBER)),
        TABLE_OP(OP_GET_MEMBER),
        TABLE_OP(EXTENDED_OP(OP_GET_MEMBER)),
        TABLE_OP(OP_SET_INDEX),
        TABLE_OP(OP_GET_INDEX),
        TABLE_OP(OP_SWAP_GLOBAL),
        TABLE_OP(EXTENDED_OP(OP_SWAP_GLOBAL)),
        TABLE_OP(OP_SWAP_LOCAL),
        TABLE_OP(OP_SUPER),
        TABLE_OP(EXTENDED_OP(OP_SUPER)),
        TABLE_OP(OP_ARRAY),
        TABLE_OP(EXTENDED_OP(OP_ARRAY)),
        TABLE_OP(OP_MAP),
        TABLE_OP(EXTENDED_OP(OP_MAP)),
        TABLE_OP(OP_IMPORT),
        TABLE_OP(EXTENDED_OP(OP_IMPORT)),
        TABLE_OP(OP_IMPORT_WILDCARD),
        TABLE_OP(EXTENDED_OP(OP_IMPORT_WILDCARD)),
        TABLE_OP(OP_END)
    };

#undef TABLE_OP

    DISPATCH();

#else

#define EXEC_OP(op)                 case op:
#define END_OP()                    break

    while (true) {
        uint8_t instruction;
#endif

#ifdef JML_TRACE_STACK
#ifdef JML_COMPUTED_GOTO
    trace_stack: {
#endif
        printf("          ");
        for (jml_value_t *slot = running->stack; slot < running->stack_top; ++slot) {
            printf("[ ");
            jml_value_print(*slot);
            printf(" ]");
        }
        printf("\n");

#ifdef JML_STEP_STACK
        jml_bytecode_instruction_disassemble(&frame->closure->function->bytecode,
            (int32_t)(frame->pc - frame->closure->function->bytecode.code));
#endif

#ifdef JML_COMPUTED_GOTO
        DISPATCH();
    }
#endif
#endif

#ifndef JML_COMPUTED_GOTO
        switch (instruction = READ_BYTE()) {
#endif
            EXEC_OP(OP_NOP) {
                /*nothing*/
                END_OP();
            }

            EXEC_OP(OP_POP) {
#ifdef JML_EVAL
                jml_value_t value = jml_vm_pop();
                if (running->frame_count - 1 == 0) {
                    if (last != NULL)
                        *last = value;
                }
#else
                jml_vm_pop();
#endif
                END_OP();
            }

            EXEC_OP(OP_POP_TWO) {
#ifdef JML_EVAL
                jml_value_t value = jml_vm_pop_two();
                if (running->frame_count - 2 == 0) {
                    if (last != NULL)
                        *last = value;
                }
#else
                jml_vm_pop_two();
#endif
                END_OP();
            }

            EXEC_OP(OP_ROT) {
                jml_vm_rot();
                END_OP();
            }

            EXEC_OP(OP_SAVE) {
                jml_vm_push(jml_vm_peek(0));
                END_OP();
            }

            EXEC_OP(OP_CONST) {
                jml_value_t constant = READ_CONST();
                jml_vm_push(constant);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_CONST)) {
                jml_value_t constant = READ_CONST_EXTENDED();
                jml_vm_push(constant);
                END_OP();
            }

            EXEC_OP(OP_NONE) {
                jml_vm_push(NONE_VAL);
                END_OP();
            }

            EXEC_OP(OP_TRUE) {
                jml_vm_push(TRUE_VAL);
                END_OP();
            }

            EXEC_OP(OP_FALSE) {
                jml_vm_push(FALSE_VAL);
                END_OP();
            }

            EXEC_OP(OP_BOOL) {
                jml_vm_push(BOOL_VAL(
                    !jml_value_falsey(jml_vm_pop())
                ));
                END_OP();
            }

            EXEC_OP(OP_ADD) {
                BINARY_OP(
                    NUM_VAL, +, double, "add to", vm->add_string
                );
                END_OP();
            }

            EXEC_OP(OP_SUB) {
                BINARY_OP(
                    NUM_VAL, -, double, "subtract from", vm->sub_string
                );
                END_OP();
            }

            EXEC_OP(OP_MUL) {
                BINARY_OP(
                    NUM_VAL, *, double, "multiply", vm->mul_string
                );
                END_OP();
            }

            EXEC_OP(OP_POW) {
                BINARY_FN(
                    NUM_VAL, pow, double, "exponentiate", vm->pow_string
                );
                END_OP();
            }

            EXEC_OP(OP_DIV) {
                BINARY_DIV(
                    NUM_VAL, /, double, "divide", vm->div_string
                );
                END_OP();
            }

            EXEC_OP(OP_MOD) {
                BINARY_DIV(
                    NUM_VAL, %, uint64_t, "divide (modulo)", vm->mod_string
                );
                END_OP();
            }

            EXEC_OP(OP_NOT) {
                jml_vm_push(
                    BOOL_VAL(jml_value_falsey(jml_vm_pop()))
                );
                END_OP();
            }

            EXEC_OP(OP_NEG) {
                if (!IS_NUM(jml_vm_peek(0))) {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Operand must be a number."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_push(
                    NUM_VAL(-AS_NUM(jml_vm_pop()))
                );
                END_OP();
            }

            EXEC_OP(OP_EQUAL) {
                jml_value_t b = jml_vm_pop();
                jml_value_t a = jml_vm_pop();
                jml_vm_push(
                    BOOL_VAL(jml_value_equal(a, b))
                );
                END_OP();
            }

            EXEC_OP(OP_GREATER) {
                BINARY_OP(
                    BOOL_VAL, >, double, "compare (gt)", vm->gt_string
                );
                END_OP();
            }

            EXEC_OP(OP_GREATEREQ) {
                BINARY_OP(
                    BOOL_VAL, >=, double, "compare (ge)", vm->ge_string
                );
                END_OP();
            }

            EXEC_OP(OP_LESS) {
                BINARY_OP(
                    BOOL_VAL, <, double, "compare (lt)", vm->lt_string
                );
                END_OP();
            }

            EXEC_OP(OP_LESSEQ) {
                BINARY_OP(
                    BOOL_VAL, <=, double, "compare (le)", vm->le_string
                );
                END_OP();
            }

            EXEC_OP(OP_NOTEQ) {
                jml_value_t b = jml_vm_pop();
                jml_value_t a = jml_vm_pop();
                jml_vm_push(
                    BOOL_VAL(!jml_value_equal(a, b))
                );
                END_OP();
            }

            EXEC_OP(OP_CONCAT) {
                jml_value_t head    = jml_vm_peek(1);
                jml_value_t tail    = jml_vm_peek(0);

                if (IS_STRING(head)) {
                    if (!IS_STRING(tail)) {
                        SAVE_FRAME();
                        RUNTIME_ERROR(
                            "DiffTypes: Can't concatenate string to %s.",
                            jml_value_stringify_type(tail)
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    jml_string_concatenate();

                } else if (IS_ARRAY(jml_vm_peek(1)))
                    jml_array_concatenate();

                else if (IS_INSTANCE(head)) {
                    SAVE_FRAME();
                    if (!jml_vm_invoke_instance(running, AS_INSTANCE(head),
                        vm->concat_string, 1)) {
                        RUNTIME_ERROR(
                            "DiffTypes: Can't concatenate instance of '%.*s'.",
                            (int32_t)AS_INSTANCE(head)->klass->name->length,
                            AS_INSTANCE(head)->klass->name->chars
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    LOAD_FRAME();
                    END_OP();

                } else {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Operands must be strings, array or instances."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_CONTAIN) {
                jml_value_t box     = jml_vm_peek(0);
                jml_value_t value   = jml_vm_peek(1);

                if (IS_ARRAY(box)) {
                    jml_obj_array_t *array = AS_ARRAY(box);
                    for (int i = 0; i < array->values.count; ++i) {
                        if (jml_value_equal(value, array->values.values[i])) {
                            jml_vm_pop_two();
                            jml_vm_push(TRUE_VAL);
                            END_OP();
                        }
                    }
                } else {
                    SAVE_FRAME();
                    RUNTIME_ERROR("WrongValue: Container must be iterable.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_pop_two();
                jml_vm_push(FALSE_VAL);
                END_OP();
            }

            EXEC_OP(OP_JUMP) {
                uint16_t offset = READ_SHORT();
                pc += offset;
                END_OP();
            }

            EXEC_OP(OP_JUMP_IF_FALSE) {
                uint16_t offset     = READ_SHORT();
                if (jml_value_falsey(jml_vm_peek(0)))
                    pc += offset;
                END_OP();
            }

            EXEC_OP(OP_LOOP) {
                uint16_t offset     = READ_SHORT();
                pc -= offset;
                END_OP();
            }

            EXEC_OP(OP_CALL) {
                int arg_count       = READ_BYTE();
                SAVE_FRAME();
                if (!jml_vm_call_value(running, jml_vm_peek(arg_count), arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_TRY_CALL) {
                int arg_count       = READ_BYTE();
                ++pc;

                SAVE_FRAME();
                if (!jml_vm_call_value(running, jml_vm_peek(arg_count), arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_INVOKE) {
                jml_obj_string_t *name      = READ_STRING();
                int               arg_count = READ_BYTE();

                SAVE_FRAME();
                if (!jml_vm_invoke(running, name, arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_INVOKE)) {
                jml_obj_string_t *name      = READ_STRING_EXTENDED();
                int               arg_count = READ_SHORT();

                SAVE_FRAME();
                if (!jml_vm_invoke(running, name, arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_TRY_INVOKE) {
                jml_obj_string_t *name      = READ_STRING();
                int               arg_count = READ_BYTE();
                ++pc;

                SAVE_FRAME();
                if (!jml_vm_invoke(running, name, arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_TRY_INVOKE)) {
                jml_obj_string_t *name      = READ_STRING_EXTENDED();
                int               arg_count = READ_SHORT();
                ++pc;

                SAVE_FRAME();
                if (!jml_vm_invoke(running, name, arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_SUPER_INVOKE) {
                jml_obj_string_t *method    = READ_STRING();
                int               arg_count = READ_BYTE();

                SAVE_FRAME();
                jml_obj_class_t *superclass = AS_CLASS(jml_vm_pop());
                if (!jml_vm_invoke_class(running, superclass, method, arg_count)) {
                    RUNTIME_ERROR(
                        "UndefErr: Undefined property '%.*s'.",
                        (int32_t)method->length, method->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_SUPER_INVOKE)) {
                jml_obj_string_t *method    = READ_STRING_EXTENDED();
                int               arg_count = READ_SHORT();

                SAVE_FRAME();
                jml_obj_class_t *superclass = AS_CLASS(jml_vm_pop());
                if (!jml_vm_invoke_class(running, superclass, method, arg_count)) {
                    RUNTIME_ERROR(
                        "UndefErr: Undefined property '%.*s'.",
                        (int32_t)method->length, method->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_TRY_SUPER_INVOKE) {
                jml_obj_string_t *method    = READ_STRING();
                int               arg_count = READ_BYTE();
                ++pc;

                SAVE_FRAME();
                jml_obj_class_t *superclass = AS_CLASS(jml_vm_pop());
                if (!jml_vm_invoke_class(running, superclass, method, arg_count)) {
                    RUNTIME_ERROR(
                        "UndefErr: Undefined property '%.*s'.",
                        (int32_t)method->length, method->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_TRY_SUPER_INVOKE)) {
                jml_obj_string_t *method    = READ_STRING_EXTENDED();
                int               arg_count = READ_SHORT();
                ++pc;

                SAVE_FRAME();
                jml_obj_class_t *superclass = AS_CLASS(jml_vm_pop());
                if (!jml_vm_invoke_class(running, superclass, method, arg_count)) {
                    RUNTIME_ERROR(
                        "UndefErr: Undefined property '%.*s'.",
                        (int32_t)method->length, method->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_CLOSURE) {
                jml_obj_function_t *function = AS_FUNCTION(READ_CONST());
                jml_obj_closure_t  *closure  = jml_obj_closure_new(function);
                jml_vm_push(OBJ_VAL(closure));

                for (int i = 0; i < closure->upvalue_count; ++i) {
                    uint8_t local = READ_BYTE();
                    uint8_t index = READ_BYTE();

                    if (local)
                        closure->upvalues[i] = jml_vm_upvalue_capture(running, frame->slots + index);
                    else
                        closure->upvalues[i] = frame->closure->upvalues[index];
                }
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_CLOSURE)) {
                jml_obj_function_t *function = AS_FUNCTION(READ_CONST_EXTENDED());
                jml_obj_closure_t  *closure  = jml_obj_closure_new(function);
                jml_vm_push(OBJ_VAL(closure));

                for (int i = 0; i < closure->upvalue_count; ++i) {
                    uint8_t local = READ_SHORT();
                    uint8_t index = READ_SHORT();

                    if (local)
                        closure->upvalues[i] = jml_vm_upvalue_capture(running, frame->slots + index);
                    else
                        closure->upvalues[i] = frame->closure->upvalues[index];
                }
                END_OP();
            }

            EXEC_OP(OP_RETURN) {
                jml_value_t result          = jml_vm_pop();
                jml_vm_upvalue_close(running, frame->slots);
                --running->frame_count;

                if (running->frame_count == 0) {
                    jml_vm_pop();
                    return INTERPRET_OK;
                }

                running->stack_top = frame->slots;
                jml_vm_push(result);

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_SPREAD) {
                SAVE_FRAME();
                if (!IS_EXCEPTION(jml_vm_peek(0))) {
                    jml_vm_error("Can spread only exceptions.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!jml_vm_exception(AS_EXCEPTION(jml_vm_peek(0))))
                    return INTERPRET_RUNTIME_ERROR;

                jml_vm_pop();
                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_CLASS) {
                jml_vm_push(
                    OBJ_VAL(jml_obj_class_new(READ_STRING()))
                );
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_CLASS)) {
                jml_vm_push(
                    OBJ_VAL(jml_obj_class_new(READ_STRING_EXTENDED()))
                );
                END_OP();
            }

            EXEC_OP(OP_CLASS_FIELD) {
                jml_vm_class_field_define(READ_STRING());
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_CLASS_FIELD)) {
                jml_vm_class_field_define(READ_STRING_EXTENDED());
                END_OP();
            }

            EXEC_OP(OP_INHERIT) {
                jml_value_t superclass = jml_vm_peek(1);
                if (!IS_CLASS(superclass)) {
                    SAVE_FRAME();
                    RUNTIME_ERROR("WrongValue: Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!AS_CLASS(superclass)->inheritable) {
                    SAVE_FRAME();
                    RUNTIME_ERROR("WrongValue: Superclass must be inheritable.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_obj_class_t *subclass   = AS_CLASS(jml_vm_peek(0));
                subclass->super             = AS_CLASS(superclass);

                jml_hashmap_add(
                    &AS_CLASS(superclass)->statics,
                    &subclass->statics
                );
                jml_vm_pop();
                END_OP();
            }

            EXEC_OP(OP_SET_LOCAL) {
                uint8_t slot = READ_BYTE();

#ifdef JML_TRACE_SLOT
                printf("          (slot %d)     [ ", slot);
                jml_value_print(frame->slots[slot]);
                printf(" ]     ->     [ ");
                jml_value_print(jml_vm_peek(0));
                printf(" ]\n");
#endif
                frame->slots[slot] = jml_vm_peek(0);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_SET_LOCAL)) {
                uint16_t slot = READ_SHORT();

#ifdef JML_TRACE_SLOT
                printf("          (slot %d)     [ ", slot);
                jml_value_print(frame->slots[slot]);
                printf(" ]     ->     [ ");
                jml_value_print(jml_vm_peek(0));
                printf(" ]\n");
#endif
                frame->slots[slot] = jml_vm_peek(0);
                END_OP();
            }

            EXEC_OP(OP_GET_LOCAL) {
                uint8_t slot = READ_BYTE();

#ifdef JML_TRACE_SLOT
                printf("          (slot %d)     [ ", slot);
                jml_value_print(frame->slots[slot]);
                printf(" ]\n");
#endif
                jml_vm_push(frame->slots[slot]);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_GET_LOCAL)) {
                uint16_t slot = READ_SHORT();

#ifdef JML_TRACE_SLOT
                printf("          (slot %d)     [ ", slot);
                jml_value_print(frame->slots[slot]);
                printf(" ]\n");
#endif
                jml_vm_push(frame->slots[slot]);
                END_OP();
            }

            EXEC_OP(OP_SET_UPVALUE) {
                uint8_t slot = READ_BYTE();

#ifdef JML_TRACE_SLOT
                printf("          (slot %d)     [ ", slot);
                if (frame->closure->upvalues[slot]->location != NULL)
                    jml_value_print(*frame->closure->upvalues[slot]->location);
                else
                    printf("(null)");

                printf(" ]     ->     [ ");
                jml_value_print(jml_vm_peek(0));
                printf(" ]\n");
#endif
                *frame->closure->upvalues[slot]->location = jml_vm_peek(0);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_SET_UPVALUE)) {
                uint16_t slot = READ_SHORT();

#ifdef JML_TRACE_SLOT
                printf("          (slot %d)     [ ", slot);
                if (frame->closure->upvalues[slot]->location != NULL)
                    jml_value_print(*frame->closure->upvalues[slot]->location);
                else
                    printf("(null)");

                printf(" ]     ->     [ ");
                jml_value_print(jml_vm_peek(0));
                printf(" ]\n");
#endif
                *frame->closure->upvalues[slot]->location = jml_vm_peek(0);
                END_OP();
            }

            EXEC_OP(OP_GET_UPVALUE) {
                uint8_t slot = READ_BYTE();

#ifdef JML_TRACE_SLOT
                printf("          (slot %d)     [ ", slot);
                jml_value_print(*frame->closure->upvalues[slot]->location);
                printf(" ]\n");
#endif
                jml_vm_push(*frame->closure->upvalues[slot]->location);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_GET_UPVALUE)) {
                uint16_t slot = READ_SHORT();

#ifdef JML_TRACE_SLOT
                printf("          (slot %d)     [ ", slot);
                jml_value_print(*frame->closure->upvalues[slot]->location);
                printf(" ]\n");
#endif
                jml_vm_push(*frame->closure->upvalues[slot]->location);
                END_OP();
            }

            EXEC_OP(OP_CLOSE_UPVALUE) {
                jml_vm_upvalue_close(running, running->stack_top - 1);
                jml_vm_pop();
                END_OP();
            }

            EXEC_OP(OP_SET_GLOBAL) {
                jml_value_t module      = READ_CONST();
                jml_obj_string_t *name  = READ_STRING();

                if (jml_vm_global_set(module, name, jml_vm_peek(0))) {
                    jml_vm_global_del(module, name);

                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "UndefErr: Undefined variable '%.*s'.",
                        (int32_t)name->length, name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_SET_GLOBAL)) {
                jml_value_t module      = READ_CONST_EXTENDED();
                jml_obj_string_t *name  = READ_STRING_EXTENDED();

                if (jml_vm_global_set(module, name, jml_vm_peek(0))) {
                    jml_vm_global_del(module, name);

                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "UndefErr: Undefined variable '%.*s'.",
                        (int32_t)name->length, name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_GET_GLOBAL) {
                jml_value_t module      = READ_CONST();
                jml_obj_string_t *name  = READ_STRING();
                jml_value_t *value;

                if (!jml_vm_global_get(module, name, &value)) {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "UndefErr: Undefined variable '%.*s'.",
                        (int32_t)name->length, name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                jml_vm_push(*value);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_GET_GLOBAL)) {
                jml_value_t module      = READ_CONST_EXTENDED();
                jml_obj_string_t *name  = READ_STRING_EXTENDED();
                jml_value_t *value;

                if (!jml_vm_global_get(module, name, &value)) {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "UndefErr: Undefined variable '%.*s'.",
                        (int32_t)name->length, name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                jml_vm_push(*value);
                END_OP();
            }

            EXEC_OP(OP_DEF_GLOBAL) {
                jml_value_t module      = READ_CONST();
                jml_obj_string_t *name  = READ_STRING();

                jml_vm_global_set(module, name, jml_vm_peek(0));
                jml_vm_pop();
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_DEF_GLOBAL)) {
                jml_value_t module      = READ_CONST_EXTENDED();
                jml_obj_string_t *name  = READ_STRING_EXTENDED();

                jml_vm_global_set(module, name, jml_vm_peek(0));
                jml_vm_pop();
                END_OP();
            }

            EXEC_OP(OP_DEL_GLOBAL) {
                jml_value_t module      = READ_CONST();
                jml_obj_string_t *name  = READ_STRING();

                jml_vm_global_del(module, name);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_DEL_GLOBAL)) {
                jml_value_t module      = READ_CONST_EXTENDED();
                jml_obj_string_t *name  = READ_STRING_EXTENDED();

                jml_vm_global_del(module, name);
                END_OP();
            }

            EXEC_OP(OP_SET_MEMBER) {
                jml_value_t              peeked = jml_vm_peek(1);

                if (IS_INSTANCE(peeked)) {
                    jml_obj_instance_t *instance = AS_INSTANCE(peeked);
                    jml_hashmap_set(
                        &instance->fields, READ_STRING(), jml_vm_peek(0)
                    );

                    jml_value_t value = jml_vm_pop();
                    jml_vm_pop();
                    jml_vm_push(value);
                    END_OP();

                } else if (IS_CLASS(peeked)) {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Class properties are immutable."
                    );
                    return INTERPRET_RUNTIME_ERROR;

                } else if (IS_MODULE(peeked)) {
                    jml_obj_module_t   *module  = AS_MODULE(peeked);
                    jml_hashmap_set(
                        &module->globals, READ_STRING(), jml_vm_peek(0)
                    );

                    jml_value_t value = jml_vm_pop();
                    jml_vm_pop();
                    jml_vm_push(value);
                    END_OP();

                } else {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Only instances and modules have properties."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_SET_MEMBER)) {
                jml_value_t              peeked = jml_vm_peek(1);

                if (IS_INSTANCE(peeked)) {
                    jml_obj_instance_t *instance = AS_INSTANCE(peeked);
                    jml_hashmap_set(
                        &instance->fields, READ_STRING_EXTENDED(), jml_vm_peek(0)
                    );

                    jml_value_t value = jml_vm_pop();
                    jml_vm_pop();
                    jml_vm_push(value);
                    END_OP();

                } else if (IS_CLASS(peeked)) {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Class properties are immutable."
                    );
                    return INTERPRET_RUNTIME_ERROR;

                } else if (IS_MODULE(peeked)) {
                    jml_obj_module_t   *module  = AS_MODULE(peeked);
                    jml_hashmap_set(
                        &module->globals, READ_STRING_EXTENDED(), jml_vm_peek(0)
                    );

                    jml_value_t value = jml_vm_pop();
                    jml_vm_pop();
                    jml_vm_push(value);
                    END_OP();

                } else {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Only instances and modules have properties."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_GET_MEMBER) {
                jml_value_t             peeked      = jml_vm_peek(0);
                jml_obj_string_t       *name        = READ_STRING();

                if (IS_INSTANCE(peeked)) {
                    jml_obj_instance_t *instance    = AS_INSTANCE(peeked);

                    jml_value_t *value;
                    if (jml_hashmap_get(&instance->fields, name, &value)) {
                        jml_vm_pop();
                        jml_vm_push(*value);
                        END_OP();
                    }

                    SAVE_FRAME();
                    if (!jml_vm_class_field_bind(instance->klass, name))
                        return INTERPRET_RUNTIME_ERROR;

                } else if (IS_CLASS(peeked)) {
                    jml_value_t *value;
                    if (jml_hashmap_get(&AS_CLASS(peeked)->statics, name, &value)) {
                        jml_vm_pop();
                        jml_vm_push(*value);
                        END_OP();
                    }

                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "UndefErr: Undefined property '%.*s'.",
                        (int32_t)name->length, name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;

                } else if (IS_MODULE(peeked)) {
                    jml_obj_module_t   *module      = AS_MODULE(peeked);

                    jml_value_t *value;
                    if (jml_hashmap_get(&module->globals, name, &value)) {
                        jml_vm_pop();
                        jml_vm_push(*value);
                        END_OP();
                    }

                    SAVE_FRAME();
                    if (!jml_vm_module_bind(module, name))
                        return INTERPRET_RUNTIME_ERROR;

                } else {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Only instances and modules have properties."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_GET_MEMBER)) {
                jml_value_t             peeked      = jml_vm_peek(0);
                jml_obj_string_t       *name        = READ_STRING_EXTENDED();

                if (IS_INSTANCE(peeked)) {
                    jml_obj_instance_t *instance    = AS_INSTANCE(peeked);

                    jml_value_t *value;
                    if (jml_hashmap_get(&instance->fields, name, &value)) {
                        jml_vm_pop();
                        jml_vm_push(*value);
                        END_OP();
                    }

                    SAVE_FRAME();
                    if (!jml_vm_class_field_bind(instance->klass, name))
                        return INTERPRET_RUNTIME_ERROR;

                } else if (IS_CLASS(peeked)) {
                    jml_value_t *value;
                    if (jml_hashmap_get(&AS_CLASS(peeked)->statics, name, &value)) {
                        jml_vm_pop();
                        jml_vm_push(*value);
                        END_OP();
                    }

                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "UndefErr: Undefined property '%.*s'.",
                        (int32_t)name->length, name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;

                } else if (IS_MODULE(peeked)) {
                    jml_obj_module_t   *module      = AS_MODULE(peeked);

                    jml_value_t *value;
                    if (jml_hashmap_get(&module->globals, name, &value)) {
                        jml_vm_pop();
                        jml_vm_push(*value);
                        END_OP();
                    }

                    SAVE_FRAME();
                    if (!jml_vm_module_bind(module, name))
                        return INTERPRET_RUNTIME_ERROR;

                } else {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Only instances and modules have properties."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_SET_INDEX) {
                jml_value_t         value   = jml_vm_peek(0);
                jml_value_t         index   = jml_vm_peek(1);
                jml_value_t         box     = jml_vm_peek(2);

                if (IS_MAP(box)) {
                    if (!IS_STRING(index)) {
                        SAVE_FRAME();
                        RUNTIME_ERROR(
                            "DiffTypes: Maps can be indexed only by strings."
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_hashmap_set(
                        &AS_MAP(box)->hashmap, AS_STRING(index), value
                    );

                } else if (IS_ARRAY(box)) {
                    if (!IS_NUM(index)) {
                        SAVE_FRAME();
                        RUNTIME_ERROR(
                            "DiffTypes: Arrays can be indexed only by numbers."
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_value_array_t array = AS_ARRAY(box)->values;
                    int num_index   = AS_NUM(index);

                    if (num_index >= array.count || num_index < -array.count) {
                        SAVE_FRAME();
                        RUNTIME_ERROR(
                            "RangeErr: Out of bounds assignment to array."
                        );
                        return INTERPRET_RUNTIME_ERROR;

                    }

                    if (num_index < 0)
                        array.values[array.count + num_index]   = value;
                    else
                        array.values[num_index]                 = value;

                } else if (IS_INSTANCE(box)) {
                    SAVE_FRAME();
                    if (!jml_vm_invoke_instance(running, AS_INSTANCE(box),
                        vm->set_string, 2)) {
                        RUNTIME_ERROR(
                            "DiffTypes: Can't index instance of '%.*s'.",
                            (int32_t)AS_INSTANCE(box)->klass->name->length,
                            AS_INSTANCE(box)->klass->name->chars
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    LOAD_FRAME();
                    END_OP();

                } else {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Can index only arrays, maps and instances."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_pop_two();
                jml_vm_pop();
                jml_vm_push(value);
                END_OP();
            }

            EXEC_OP(OP_GET_INDEX) {
                jml_value_t         index   = jml_vm_peek(0);
                jml_value_t         box     = jml_vm_peek(1);
                jml_value_t         value;

                if (IS_MAP(box)) {
                    if (!IS_STRING(index)) {
                        SAVE_FRAME();
                        RUNTIME_ERROR(
                            "DiffTypes: Maps can be indexed only by strings."
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_value_t    *temp;
                    if (!jml_hashmap_get(&AS_MAP(box)->hashmap, AS_STRING(index), &temp))
                        value       = NONE_VAL;
                    else
                        value       = *temp;

                } else if (IS_ARRAY(box)) {
                    if (!IS_NUM(index)) {
                        SAVE_FRAME();
                        RUNTIME_ERROR(
                            "DiffTypes: Arrays can be indexed only by numbers."
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_value_array_t array = AS_ARRAY(box)->values;
                    int num_index   = AS_NUM(index);

                    if (num_index >= array.count || num_index < -array.count)
                        value       = NONE_VAL;
                    else if (num_index < 0)
                        value       = array.values[array.count + num_index];
                    else
                        value       = array.values[num_index];

                } else if (IS_INSTANCE(box)) {
                    SAVE_FRAME();
                    if (!jml_vm_invoke_instance(running, AS_INSTANCE(box),
                        vm->get_string, 1)) {
                        RUNTIME_ERROR(
                            "DiffTypes: Can't index instance of '%.*s'.",
                            (int32_t)AS_INSTANCE(box)->klass->name->length,
                            AS_INSTANCE(box)->klass->name->chars
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    LOAD_FRAME();
                    END_OP();

                } else {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "DiffTypes: Can index only arrays, maps and instances."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_pop_two();
                jml_vm_push(value);
                END_OP();
            }

            EXEC_OP(OP_SWAP_GLOBAL) {
                jml_value_t module          = READ_CONST();
                jml_obj_string_t *old_name  = READ_STRING();
                jml_obj_string_t *new_name  = READ_STRING();

                jml_value_t value;
                if (!jml_vm_global_pop(module, old_name, &value)) {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "UndefErr: Undefined variable '%.*s'.",
                        (int32_t)old_name->length, old_name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_global_set(module, new_name, value);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_SWAP_GLOBAL)) {
                jml_value_t module          = READ_CONST_EXTENDED();
                jml_obj_string_t *old_name  = READ_STRING_EXTENDED();
                jml_obj_string_t *new_name  = READ_STRING_EXTENDED();

                jml_value_t value;
                if (!jml_vm_global_pop(module, old_name, &value)) {
                    SAVE_FRAME();
                    RUNTIME_ERROR(
                        "UndefErr: Undefined variable '%.*s'.",
                        (int32_t)old_name->length, old_name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_global_set(module, new_name, value);
                END_OP();
            }

            EXEC_OP(OP_SWAP_LOCAL) {
                /*placeholder*/
                END_OP();
            }

            EXEC_OP(OP_SUPER) {
                jml_obj_string_t *name       = READ_STRING();
                jml_obj_class_t  *superclass = AS_CLASS(jml_vm_pop());

                SAVE_FRAME();
                if (!jml_vm_class_field_bind(superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_SUPER)) {
                jml_obj_string_t *name       = READ_STRING_EXTENDED();
                jml_obj_class_t  *superclass = AS_CLASS(jml_vm_pop());

                SAVE_FRAME();
                if (!jml_vm_class_field_bind(superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_ARRAY) {
                uint8_t          item_count  = READ_BYTE();
                jml_value_t     *values      = running->stack_top -= item_count;
                jml_obj_array_t *array       = jml_obj_array_new();
                jml_value_t      array_value = OBJ_VAL(array);
                jml_gc_exempt_push(array_value);

                for (uint8_t i = 0; i < item_count; ++i) {
                    jml_obj_array_append(
                        array, values[i]
                    );
                }

                jml_gc_exempt_pop();
                jml_vm_push(array_value);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_ARRAY)) {
                uint16_t         item_count  = READ_SHORT();
                jml_value_t     *values      = running->stack_top -= item_count;
                jml_obj_array_t *array       = jml_obj_array_new();
                jml_value_t      array_value = OBJ_VAL(array);
                jml_gc_exempt_push(array_value);

                for (uint16_t i = 0; i < item_count; ++i) {
                    jml_obj_array_append(
                        array, values[i]
                    );
                }

                jml_gc_exempt_pop();
                jml_vm_push(array_value);
                END_OP();
            }

            EXEC_OP(OP_MAP) {
                uint8_t          item_count  = READ_BYTE();
                jml_value_t     *values      = running->stack_top -= item_count;
                jml_obj_map_t   *map         = jml_obj_map_new();
                jml_value_t      map_value   = OBJ_VAL(map);
                jml_gc_exempt_push(map_value);

                for (uint8_t i = 0; i < item_count; i += 2) {
                    if (!IS_STRING(values[i])) {
                        SAVE_FRAME();
                        RUNTIME_ERROR("DiffTypes: Map key must be a string.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_hashmap_set(
                        &map->hashmap,
                        AS_STRING(values[i]),
                        values[i + 1]
                    );
                }

                jml_gc_exempt_pop();
                jml_vm_push(map_value);
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_MAP)) {
                uint16_t         item_count  = READ_SHORT();
                jml_value_t     *values      = running->stack_top -= item_count;
                jml_obj_map_t   *map         = jml_obj_map_new();
                jml_value_t      map_value   = OBJ_VAL(map);
                jml_gc_exempt_push(map_value);

                for (uint16_t i = 0; i < item_count; i += 2) {
                    if (!IS_STRING(values[i])) {
                        SAVE_FRAME();
                        RUNTIME_ERROR("DiffTypes: Map key must be a string.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_hashmap_set(
                        &map->hashmap,
                        AS_STRING(values[i]),
                        values[i + 1]
                    );
                }

                jml_gc_exempt_pop();
                jml_vm_push(map_value);
                END_OP();
            }

            EXEC_OP(OP_IMPORT) {
                jml_obj_string_t *fullname   = READ_STRING();
                jml_obj_string_t *name       = READ_STRING();

                SAVE_FRAME();
                if (!jml_vm_module_import(fullname, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_IMPORT)) {
                jml_obj_string_t *fullname   = READ_STRING_EXTENDED();
                jml_obj_string_t *name       = READ_STRING_EXTENDED();

                SAVE_FRAME();
                if (!jml_vm_module_import(fullname, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_IMPORT_WILDCARD) {
                jml_obj_string_t *fullname   = READ_STRING();
                jml_obj_string_t *name       = READ_STRING();

                SAVE_FRAME();
                if (!jml_vm_module_import(fullname, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_hashmap_add(
                    &AS_MODULE(jml_vm_peek(0))->globals,
                    vm->current == NULL ? &vm->globals : &vm->current->globals
                );

                jml_vm_pop();
                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(EXTENDED_OP(OP_IMPORT_WILDCARD)) {
                jml_obj_string_t *fullname   = READ_STRING_EXTENDED();
                jml_obj_string_t *name       = READ_STRING_EXTENDED();

                SAVE_FRAME();
                if (!jml_vm_module_import(fullname, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_hashmap_add(
                    &AS_MODULE(jml_vm_peek(0))->globals,
                    vm->current == NULL ? &vm->globals : &vm->current->globals
                );

                jml_vm_pop();
                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_END) {
                JML_UNREACHABLE();
                END_OP();
            }

#ifndef JML_COMPUTED_GOTO
            default:
                JML_UNREACHABLE();
        }
    }
#endif


#undef SAVE_FRAME
#undef LOAD_FRAME

#undef READ_BYTE
#undef READ_SHORT
#undef READ_LONG

#undef READ_STRING
#undef READ_CSTRING
#undef READ_CONST

#undef READ_STRING_EXTENDED
#undef READ_CSTRING_EXTENDED
#undef READ_CONST_EXTENDED

#undef BINARY_OP
#undef BINARY_DIV
#undef BINARY_FN

#undef EXEC_OP_
#undef EXEC_OP
#undef END_OP_
#undef END_OP
}


jml_interpret_result
jml_vm_call_coroutine(jml_obj_coroutine_t *coroutine, jml_value_t *last)
{
    jml_obj_coroutine_t *saved = vm->running;
    jml_obj_coroutine_t *caller = coroutine->caller;

    vm->running         = coroutine;
    vm->running->caller = saved;

    jml_interpret_result result = jml_vm_run(last);

    vm->running         = saved;
    coroutine->caller   = caller;

    return result;
}


void
jml_cfunction_register(const char *name,
    jml_cfunction function, jml_obj_module_t *module)
{
    jml_gc_exempt_push(jml_string_intern(name));

    jml_gc_exempt_push(OBJ_VAL(
        jml_obj_cfunction_new(
            AS_STRING(jml_gc_exempt_peek(0)),
            function,
            module
        )
    ));

    jml_vm_global_set(
        vm->current == NULL ? NONE_VAL : OBJ_VAL(vm->current),
        AS_STRING(jml_vm_peek(1)),
        jml_gc_exempt_peek(0)
    );

    jml_gc_exempt_pop();
    jml_gc_exempt_pop();
}


jml_interpret_result
jml_vm_interpret(jml_vm_t *_vm, const char *source)
{
    vm = _vm;

    jml_obj_function_t *function = jml_compiler_compile(
        source, NULL, true);

    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    jml_gc_exempt_push(OBJ_VAL(function));
    jml_obj_closure_t *closure = jml_obj_closure_new(function);
    jml_gc_exempt_push(OBJ_VAL(closure));

    jml_vm_global_set(NONE_VAL, vm->module_string, OBJ_VAL(vm->main_string));
    jml_vm_global_set(NONE_VAL, vm->path_string, NONE_VAL);

    vm->running = jml_obj_coroutine_new(closure);
    jml_gc_exempt_pop();
    jml_gc_exempt_pop();

    return jml_vm_run(NULL);
}


jml_interpret_result
jml_vm_interpret_bytecode(jml_vm_t *_vm, jml_bytecode_t *bytecode)
{
    vm = _vm;

    jml_obj_function_t *function = jml_obj_function_new();
    memcpy(&function->bytecode, bytecode, sizeof(jml_bytecode_t));

    jml_gc_exempt_push(OBJ_VAL(function));
    jml_obj_closure_t *closure = jml_obj_closure_new(function);
    jml_gc_exempt_push(OBJ_VAL(closure));

    jml_vm_global_set(NONE_VAL, vm->module_string, OBJ_VAL(vm->main_string));
    jml_vm_global_set(NONE_VAL, vm->path_string, NONE_VAL);

    vm->running = jml_obj_coroutine_new(closure);
    jml_gc_exempt_pop();
    jml_gc_exempt_pop();

    return jml_vm_run(NULL);
}


jml_value_t
jml_vm_eval(jml_vm_t *_vm, const char *source)
{
    vm = _vm;

#ifdef JML_EVAL
    jml_obj_function_t *function = jml_compiler_compile(
        source, NULL, true);

    if (function == NULL)
        return NONE_VAL;

    jml_gc_exempt_push(OBJ_VAL(function));
    jml_obj_closure_t *closure = jml_obj_closure_new(function);
    jml_gc_exempt_push(OBJ_VAL(closure));

    jml_vm_global_set(NONE_VAL, vm->module_string, OBJ_VAL(vm->main_string));
    jml_vm_global_set(NONE_VAL, vm->path_string, NONE_VAL);

    vm->running = jml_obj_coroutine_new(closure);
    jml_gc_exempt_pop();
    jml_gc_exempt_pop();

    jml_value_t result = NONE_VAL;
    if (jml_vm_run(&result) != INTERPRET_OK)
        return NONE_VAL;

    return result;
#else
    (void) source;
    return NONE_VAL;
#endif
}
