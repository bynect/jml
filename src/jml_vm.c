#include <stdio.h>
#include <stdarg.h>

#include <jml_common.h>
#include <jml_compiler.h>
#include <jml_value.h>
#include <jml_type.h>
#include <jml_vm.h>
#include <jml_gc.h>


extern jml_vm_t vm;


static void
jml_vm_stack_reset(jml_vm_t *vm)
{
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
}


static void
jml_vm_error_runtime(const char *format, ...)
{
    va_list params;
    va_start(params, format);
    vfprintf(stderr, format, params);
    va_end(params);
    fputs("\n", stderr);

#ifdef JML_TRACE_BACK
    for (int i = 0; i < vm->frame_count; i++) {
#else
    for (int i = vm.frame_count - 1; i >= 0; i--) {
#endif
        jml_call_frame_t    *frame    = &(vm.frames[i]);
        jml_obj_function_t  *function = frame->closure->function;

        size_t instruction = frame->pc - function->bytecode.code - 1;

        fprintf(stderr, "[line %d] in ",
            function->bytecode.lines[instruction]
        );

        if (function->name == NULL) {
            printf(stderr, "script");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    jml_vm_stack_reset(&vm);
}


void
jml_vm_init(jml_vm_t *vm)
{
    jml_vm_stack_reset(vm);

    jml_gc_t gc;
    jml_gc_init(&gc, vm);
    vm->gc = gc;

    vm->init_string = NULL;
    vm->call_string = NULL;

    vm->init_string = jml_obj_string_copy("__init", 6);
    vm->call_string = jml_obj_string_copy("__call", 6);
}


void
jml_vm_free(jml_vm_t *vm)
{
    jml_hashmap_free(&(vm->strings));
    jml_hashmap_free(&(vm->globals));
    vm->init_string = NULL;

    jml_gc_free_objs();
    jml_gc_free(&(vm->gc));
}

void
jml_vm_push(jml_value_t value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}


jml_value_t
jml_vm_pop()
{
    vm.stack_top--;
    return *vm.stack_top;
}


static void
jml_vm_pop_two()
{
    vm.stack_top -= 2;
}


static jml_value_t
jml_vm_peek(int distance)
{
  return vm.stack_top[-1 - distance];
}


static bool
jml_vm_call(jml_obj_closure_t *closure,
    int arg_count)
{
    if (arg_count != closure->function->arity) {
        jml_vm_error_runtime("Expected %d arguments but got %d.",
            closure->function->arity, arg_count
        );
        return false;
    }

    if (vm.frame_count == FRAMES_MAX) {
        jml_vm_error_runtime("Recursion depth overflow.");
        return false;
    }

    jml_call_frame_t *frame = &(vm.frames[vm.frame_count++]);
    frame->closure = closure;
    frame->pc = closure->function->bytecode.code;

    frame->slots = vm.stack_top - arg_count - 1;
    return true;
}


static bool
jml_vm_call_value(jml_value_t callee, int arg_count)
{
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CFUNCTION:
                jml_cfunction cfunction = AS_CFUNCTION(callee);
                jml_value_t result = cfunction(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count + 1;
                push(result);
                return true;

            case OBJ_CLASS:
                jml_obj_class_t *klass = AS_CLASS(callee);
                vm.stack_top[-arg_count - 1] = OBJ_VAL(jml_obj_instance_new(klass));
                jml_value_t initializer;

                if (jml_hashmap_get(&klass->methods, vm.init_string, &initializer)) {
                return jml_vm_call(AS_CLOSURE(initializer), arg_count);
                } else if (arg_count != 0) {
                    jml_vm_error_runtime(
                        "Expected 0 arguments but got %d.", arg_count
                    );
                    return false;
                }
                return true;

            case OBJ_METHOD:
                jml_obj_method_t *bound = AS_METHOD(callee);
                vm.stack_top[-arg_count - 1] = bound->receiver;
                return jml_vm_call(bound->method, arg_count);

            case OBJ_CLOSURE:
                return jml_vm_call(AS_CLOSURE(callee), arg_count);

            case OBJ_INSTANCE:
                jml_obj_instance_t *instance = AS_INSTANCE(callee);
                vm.stack_top[-arg_count - 1] = OBJ_VAL(jml_obj_instance_new(klass));

                jml_value_t caller;

                if (jml_hashmap_get(&(instance->klass->methods),
                    vm.call_string, &caller)) {
                    
                    return jml_vm_call(AS_CLOSURE(caller), arg_count);
                } else if (arg_count != 0) {
                    jml_vm_error_runtime(
                        "Expected 0 arguments but got %d.", arg_count
                    );
                } else {
                    jml_vm_error_runtime(
                        "Instance of '%s' is not callable.", instance->klass->name
                    );
                }
                return true;

            default: break;
        }
    }

    jml_vm_error_runtime(
        "Can only call functions, classes and instances."
    );
    return false;
}


static bool
jml_vm_invoke_class(jml_obj_class_t *klass,
    jml_obj_string_t *name, int arg_count)
{
    jml_value_t method;

    if (!jml_hashmap_get(&(klass->methods), name, &method)) {
        jml_vm_error_runtime("Undefined property '%s'.", name->chars);
        return false;
    }

    return jml_vm_call(AS_CLOSURE(method), arg_count);
}


static bool
jml_vm_invoke_(jml_obj_string_t *name, int arg_count)
{
    jml_value_t receiver = jml_vm_peek(arg_count);

    if (!IS_INSTANCE(receiver)) {
        jml_vm_error_runtime("Only instances have methods.");
        return false;
    }

    jml_obj_instance_t *instance = AS_INSTANCE(receiver);

    jml_value_t value;
    if (jml_hashmap_get(&(instance->fields), name, &value)) {
        vm.stack_top[-arg_count - 1] = value;
        return jml_vm_call_value(value, arg_count);
    }

    return jml_vm_invoke_class(instance->klass,
        name, arg_count);
}


static bool
jml_vm_method_bind(jml_obj_class_t *klass,
    jml_obj_string_t *name)
{
    jml_value_t method;
    if (!jml_hashmap_get(&(klass->methods),
        name, &method)) {

        jml_vm_error_runtime(
            "Undefined property '%s'.", name->chars
        );
        return false;
    }

    jml_obj_method_t *bound = jml_obj_method_new(
        jml_vm_peek(0), AS_CLOSURE(method)
    );

    jml_vm_pop();
    jml_vm_push(OBJ_VAL(bound));
    return true;
}


static void
jml_vm_method_define(jml_obj_string_t *name)
{
    jml_value_t method = jml_vm_peek(0);
    jml_obj_class_t *klass = AS_CLASS(jml_vm_peek(1));
    jml_hashmap_set(&(klass->methods), name, method);
    jml_vm_pop();
}


static jml_obj_upvalue_t *
jml_vm_upvalue_capture(jml_value_t *local)
{
    jml_obj_upvalue_t *previous = NULL;
    jml_obj_upvalue_t *upvalue = vm.open_upvalues;

    while (upvalue != NULL && upvalue->location > local) {
        previous = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    jml_obj_upvalue_t *new_upvalue = jml_obj_upvalue_new(local);
    new_upvalue->next = upvalue;

    if (previous == NULL) {
        vm.open_upvalues = new_upvalue;
    } else {
        previous->next = new_upvalue;
    }

    return new_upvalue;
}


static void
jml_vm_upvalue_close(jml_value_t *last) {
    while ((vm.open_upvalues != NULL )
        && (vm.open_upvalues->location >= last)) {

        jml_obj_upvalue_t *upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &(upvalue->closed);
        vm.open_upvalues = upvalue->next;
    }
}


static void
jml_string_concatenate()
{
    jml_obj_string_t *b = AS_STRING(jml_vm_peek(0));
    jml_obj_string_t *a = AS_STRING(jml_vm_peek(1));

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);

    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    jml_obj_string_t *result = jml_obj_string_take(chars, length);
    jml_vm_pop_two();
    jml_vm_push(OBJ_VAL(result));
}


static jml_interpret_result
jml_vm_run_code()
{
    
}


jml_interpret_result
jml_vm_interpret(const char *source)
{
    jml_obj_function_t *function = jml_compiler_compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    jml_vm_push(OBJ_VAL(function));
    jml_obj_closure_t *closure = jml_obj_closure_new(function);

    jml_vm_pop();
    jml_vm_push(OBJ_VAL(closure));
    jml_vm_call_value(OBJ_VAL(closure), 0);

    return jml_vm_run_code();
}
