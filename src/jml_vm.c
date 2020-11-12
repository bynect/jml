#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>

#include <jml.h>

#include <jml_common.h>
#include <jml_vm.h>
#include <jml_gc.h>
#include <jml_compiler.h>
#include <jml_type.h>
#include <jml_module.h>


jml_vm_t *vm;


static void
jml_vm_stack_reset(void)
{
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
}


static void
jml_vm_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

#ifdef JML_TRACE_BACK
    for (int i = 0; i < vm->frame_count; i++) {
#else
    for (int i = vm->frame_count - 1; i >= 0; i--) {
#endif
        jml_call_frame_t    *frame    = &vm->frames[i];
        jml_obj_function_t  *function = frame->closure->function;

        size_t instruction = frame->pc - function->bytecode.code - 1;

        fprintf(stderr, "[line %d] in ",
            function->bytecode.lines[instruction]
        );

        if (function->name == NULL) {
            if (vm->external != NULL)
                fprintf(stderr, "function %s()\n", vm->external->chars);
            else
                fprintf(stderr, "__main\n");
        } else {
            fprintf(stderr, "function %s()\n", function->name->chars);
        }
    }

    jml_vm_stack_reset();
}


static void
jml_vm_exception(jml_obj_cfunction_t *cfunction,
    jml_obj_exception_t *exc)
{
    jml_vm_error(
        "%.*s: %.*s",
        (int)exc->name->length, exc->name->chars,
        (int)exc->message->length, exc->message->chars
    );
}


jml_vm_t *
jml_vm_new(void)
{
    jml_vm_t *vm_ptr = (jml_vm_t*)jml_realloc(NULL, sizeof(jml_vm_t));

    memset(vm_ptr, 0, sizeof(jml_vm_t));

    vm = vm_ptr;

    jml_vm_init(vm_ptr);

    return vm_ptr;
}


void
jml_vm_init(jml_vm_t *vm_ptr)
{
    jml_vm_stack_reset();

    vm_ptr->objects         = NULL;
    vm_ptr->allocated       = 0;
    vm_ptr->next_gc         = 1024 * 1024 * 4;

    vm_ptr->gray_count      = 0;
    vm_ptr->gray_capacity   = 0;
    vm_ptr->gray_stack      = NULL;

    jml_hashmap_init(&vm_ptr->strings);
    jml_hashmap_init(&vm_ptr->globals);

    vm_ptr->init_string     = NULL;
    vm_ptr->call_string     = NULL;
    vm_ptr->init_string     = jml_obj_string_copy("__init", 6);
    vm_ptr->call_string     = jml_obj_string_copy("__call", 6);

    vm_ptr->external        = NULL;
    jml_core_register();
}


void
jml_vm_free(jml_vm_t *vm_ptr)
{
    jml_hashmap_free(&vm_ptr->strings);
    jml_hashmap_free(&vm_ptr->globals);

    vm_ptr->init_string = NULL;
    vm_ptr->call_string = NULL;
    vm_ptr->external    = NULL;

    jml_gc_free_objs();

    ASSERT(
        vm_ptr->allocated == 0,
        "%zd bytes not freed\n",
        vm_ptr->allocated
    );

    free(vm_ptr);
}


void
jml_vm_push(jml_value_t value)
{
    *vm->stack_top = value;
    vm->stack_top++;
}


jml_value_t
jml_vm_pop(void)
{
    vm->stack_top--;
    return *vm->stack_top;
}


static void
jml_vm_pop_two(void)
{
    vm->stack_top -= 2;
}


static jml_value_t
jml_vm_peek(int distance)
{
  return vm->stack_top[-1 - distance];
}


static bool
jml_vm_call(jml_obj_closure_t *closure,
    int arg_count)
{
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

    if (vm->frame_count == FRAMES_MAX) {
        jml_vm_error("Scope depth overflow.");
        return false;
    }

    jml_call_frame_t *frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->pc = closure->function->bytecode.code;

    frame->slots = vm->stack_top - arg_count - 1;
    return true;
}


static bool
jml_vm_call_value(jml_value_t callee, int arg_count)
{
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_METHOD: {
                jml_obj_method_t *bound         = AS_METHOD(callee);
                vm->stack_top[-arg_count - 1]   = bound->receiver;
                return jml_vm_call(bound->method, arg_count);
            }

            case OBJ_CLOSURE: {
                return jml_vm_call(AS_CLOSURE(callee), arg_count);
            }

            case OBJ_CLASS: {
                jml_obj_class_t *klass          = AS_CLASS(callee);
                vm->stack_top[-arg_count - 1]   = OBJ_VAL(
                    jml_obj_instance_new(klass));
                jml_value_t initializer;

                if (jml_hashmap_get(&klass->methods,
                    vm->init_string, &initializer)) {

                    return jml_vm_call(AS_CLOSURE(initializer), arg_count);
                } else if (arg_count != 0) {
                    jml_vm_error(
                        "Expected 0 arguments but got %d.", arg_count
                    );
                    return false;
                }
                return true;
            }

            case OBJ_INSTANCE: {
                jml_obj_instance_t *instance    = AS_INSTANCE(callee);
                jml_value_t caller;

                if (jml_hashmap_get(&instance->klass->methods,
                    vm->call_string, &caller)) {

                    return jml_vm_call(AS_CLOSURE(caller), arg_count);
                } else {
                    jml_vm_error(
                        "Instance of class '%s' is not callable.",
                        instance->klass->name->chars
                    );
                    return false;
                }
            }

            case OBJ_CFUNCTION: {
                jml_obj_cfunction_t *cfunction_obj  = AS_CFUNCTION(callee);
                jml_cfunction        cfunction      = cfunction_obj->function;

                jml_value_t result  = cfunction(arg_count, vm->stack_top - arg_count);
                vm->stack_top       -= arg_count + 1;

                if (IS_EXCEPTION(result)) {
                    vm->external = cfunction_obj->name;
                    jml_vm_exception(cfunction_obj, AS_EXCEPTION(result));
                    return false;
                } else {
                    jml_vm_push(result);
                    return true;
                }
            }

            default: break;
        }
    }

    jml_vm_error(
        "Can only call functions, classes and instances."
    );
    return false;
}


static bool
jml_vm_invoke_class(jml_obj_class_t *klass,
    jml_obj_string_t *name, int arg_count)
{
    jml_value_t method;

    if (!jml_hashmap_get(&klass->methods, name, &method)) {
        jml_vm_error("Undefined property '%s'.", name->chars);
        return false;
    }

    return jml_vm_call(AS_CLOSURE(method), arg_count);
}


static bool
jml_vm_invoke(jml_obj_string_t *name, int arg_count)
{
    jml_value_t receiver = jml_vm_peek(arg_count);

    if (!IS_INSTANCE(receiver)) {
        jml_vm_error("Only instances have methods.");
        return false;
    }

    jml_obj_instance_t *instance = AS_INSTANCE(receiver);

    jml_value_t value;
    if (jml_hashmap_get(&instance->fields, name, &value)) {
        vm->stack_top[-arg_count - 1] = value;
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
    if (!jml_hashmap_get(&klass->methods,
        name, &method)) {

        jml_vm_error(
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
    jml_hashmap_set(&klass->methods, name, method);
    jml_vm_pop();
}


static jml_obj_upvalue_t *
jml_vm_upvalue_capture(jml_value_t *local)
{
    jml_obj_upvalue_t *previous = NULL;
    jml_obj_upvalue_t *upvalue = vm->open_upvalues;

    while (upvalue != NULL 
        && upvalue->location > local) {

        previous = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL
        && upvalue->location == local) {

        return upvalue;
    }

    jml_obj_upvalue_t *new_upvalue = jml_obj_upvalue_new(local);
    new_upvalue->next = upvalue;

    if (previous == NULL) {
        vm->open_upvalues   = new_upvalue;
    } else {
        previous->next      = new_upvalue;
    }

    return new_upvalue;
}


static void
jml_vm_upvalue_close(jml_value_t *last) {
    while ((vm->open_upvalues != NULL )
        && (vm->open_upvalues->location >= last)) {

        jml_obj_upvalue_t *upvalue  = vm->open_upvalues;
        upvalue->closed             = *upvalue->location;
        upvalue->location           = &upvalue->closed;
        vm->open_upvalues           = upvalue->next;
    }
}


static void
jml_string_concatenate(void)
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
jml_vm_run(void)
{
    jml_call_frame_t *frame = &vm->frames[vm->frame_count - 1];
    register uint8_t *pc = frame->pc;

#define READ_BYTE()                 (*pc++)
#define READ_SHORT()                (pc += 2, (uint16_t)((pc[-2] << 8) | pc[-1]))
#define READ_STRING()               AS_STRING(READ_CONST())
#define READ_CONST()                                    \
    (frame->closure->function->bytecode.constants.values[READ_BYTE()])

#define BINARY_OP(type, op, num_type)                   \
    do {                                                \
        if (!IS_NUM(jml_vm_peek(0))                     \
            || !IS_NUM(jml_vm_peek(1))) {               \
            frame->pc = pc;                             \
            jml_vm_error(                               \
                "Operands must be numbers."             \
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        num_type b = AS_NUM(jml_vm_pop());              \
        num_type a = AS_NUM(jml_vm_pop());              \
        jml_vm_push(type(a op b));                      \
    } while (false)

#define BINARY_DIV(type, op, num_type)                  \
    do {                                                \
        if (!IS_NUM(jml_vm_peek(0))                     \
            || !IS_NUM(jml_vm_peek(1))) {               \
            frame->pc = pc;                             \
            jml_vm_error(                               \
                "Operands must be numbers."             \
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        num_type b = AS_NUM(jml_vm_pop());              \
        num_type a = AS_NUM(jml_vm_pop());              \
        if (b == 0) {                                   \
            frame->pc = pc;                             \
            jml_vm_error(                               \
                "Cannot divide by zero."                \
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        jml_vm_push(type(a op b));                      \
    } while (false)

#define BINARY_FN(type, fn, num_type)                   \
    do {                                                \
        if (!IS_NUM(jml_vm_peek(0))                     \
            || !IS_NUM(jml_vm_peek(1))) {               \
            frame->pc = pc;                             \
            jml_vm_error(                               \
                "Operands must be numbers."             \
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        num_type b = AS_NUM(jml_vm_pop());              \
        num_type a = AS_NUM(jml_vm_pop());              \
        jml_vm_push(type(fn(a, b)));                    \
    } while (false)

    for ( ;; ) {
        uint8_t instruction;

        switch (instruction = READ_BYTE()) {
            case OP_POP: {
                jml_vm_pop();
                break;
            }

            case OP_POP_TWO: {
                jml_vm_pop_two();
                break;
            }

            case OP_ROT: {
                jml_value_t a       = jml_vm_pop();
                jml_value_t b       = jml_vm_pop();
                jml_vm_push(a);
                jml_vm_push(b);
                break;
            }

            case OP_CONST: {
                jml_value_t constant = READ_CONST();
                jml_vm_push(constant);
                break;
            }

            case OP_NONE: {
                jml_vm_push(NONE_VAL);
                break;
            }

            case OP_TRUE: {
                jml_vm_push(BOOL_VAL(true));
                break;
            }

            case OP_FALSE: {
                jml_vm_push(BOOL_VAL(false));
                break;
            }

            case OP_ADD: {
                if (IS_STRING(jml_vm_peek(0))
                    && IS_STRING(jml_vm_peek(1))) {
                    
                    jml_string_concatenate();
                } else if(IS_NUM(jml_vm_peek(0))
                        && IS_NUM(jml_vm_peek(1))) {

                    BINARY_OP(NUM_VAL, +, double);
                } else {
                    frame->pc = pc;
                    jml_vm_error(
                        "Operands must be two numbers or two strings."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SUB: {
                BINARY_OP(NUM_VAL, -, double);
                break;
            }

            case OP_MUL: {
                BINARY_OP(NUM_VAL, *, double);
                break;
            }

            case OP_DIV: {
                BINARY_DIV(NUM_VAL, /, double);
                break;
            }

            case OP_MOD: {
                BINARY_DIV(NUM_VAL, %, int);
                break;
            }

            case OP_POW: {
                BINARY_FN(NUM_VAL, pow, double);
                break;
            }

            case OP_NOT: {
                jml_vm_push(
                    BOOL_VAL(jml_is_falsey(jml_vm_pop()))
                );
                break;
            }

            case OP_NEGATE: {
                if (!IS_NUM(jml_vm_peek(0))) {
                    frame->pc = pc;
                    jml_vm_error(
                        "Operand must be a number."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                jml_vm_push(
                    NUM_VAL(-AS_NUM(jml_vm_pop()))
                );
                break;
            }

            case OP_EQUAL: {
                jml_value_t b = jml_vm_pop();
                jml_value_t a = jml_vm_pop();
                jml_vm_push(
                    BOOL_VAL(jml_value_equal(a, b))
                );
                break;
            }

            case OP_GREATER: {
                BINARY_OP(BOOL_VAL, >, double);
                break;
            }

            case OP_GREATEREQ: {
                BINARY_OP(BOOL_VAL, >=, double);
                break;
            }

            case OP_LESS: {
                BINARY_OP(BOOL_VAL, <, double);
                break;
            }

            case OP_LESSEQ: {
                BINARY_OP(BOOL_VAL, <=, double);
                break;
            }

            case OP_NOTEQ: {
                jml_value_t b = jml_vm_pop();
                jml_value_t a = jml_vm_pop();
                jml_vm_push(
                    BOOL_VAL(!jml_value_equal(a, b))
                );
                break;
            }

            case OP_JMP: {
                uint16_t offset = READ_SHORT();
                pc += offset;
                break;
            }

            case OP_JMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (jml_is_falsey(jml_vm_peek(0))) pc += offset;
                break;
            }

            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                pc -= offset;
                break;
            }

            case OP_CALL: {
                int arg_count = READ_BYTE();
                frame->pc = pc;
                if (!jml_vm_call_value(jml_vm_peek(arg_count), arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                frame = &vm->frames[vm->frame_count - 1];
                pc = frame->pc;
                break;
            }

            case OP_METHOD: {
                frame->pc = pc;
                jml_vm_method_define(READ_STRING());
                break;
            }
            
            case OP_INVOKE: {
                jml_obj_string_t *method = READ_STRING();
                int arg_count = READ_BYTE();
                frame->pc = pc;
                if (!jml_vm_invoke(method, arg_count))
                    return INTERPRET_RUNTIME_ERROR;
                
                frame = &vm->frames[vm->frame_count - 1];
                pc = frame->pc;
                break;
            }

            case OP_SUPER_INVOKE: {
                jml_obj_string_t *method = READ_STRING();
                int arg_count = READ_BYTE();
                frame->pc = pc;

                jml_obj_class_t *superclass = AS_CLASS(jml_vm_pop());
                if (!jml_vm_invoke_class(superclass, method, arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                frame = &vm->frames[vm->frame_count - 1];
                pc = frame->pc;
                break;
            }

            case OP_CLOSURE: {
                jml_obj_function_t *function = AS_FUNCTION(READ_CONST());
                jml_obj_closure_t *closure = jml_obj_closure_new(function);
                jml_vm_push(OBJ_VAL(closure));

                for (int i = 0; i < closure->upvalue_count; i++) {
                    uint8_t local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (local)
                        closure->upvalues[i] = jml_vm_upvalue_capture(frame->slots + index);
                    else
                        closure->upvalues[i] = frame->closure->upvalues[index];
                }
                break;
            }

            case OP_RETURN: {
                jml_value_t result = jml_vm_pop();
                jml_vm_upvalue_close(frame->slots);
                vm->frame_count--;

                if (vm->frame_count == 0) {
                    jml_vm_pop();
                    return INTERPRET_OK;
                }

                vm->stack_top = frame->slots;
                jml_vm_push(result);
                frame = &vm->frames[vm->frame_count - 1];
                pc = frame->pc;
                break;
            }

            case OP_CLASS: {
                jml_vm_push(
                    OBJ_VAL(jml_obj_class_new(READ_STRING()))
                );
                break;
            }

            case OP_INHERIT: {
                jml_value_t superclass = jml_vm_peek(1);
                if (!IS_CLASS(superclass)) {
                    frame->pc = pc;
                    jml_vm_error("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_obj_class_t *subclass   = AS_CLASS(jml_vm_peek(0));
                subclass->super             = AS_CLASS(superclass);

                jml_hashmap_add(
                    &AS_CLASS(superclass)->methods,
                    &subclass->methods
                );
                jml_vm_pop();
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = jml_vm_peek(0);
                break;
            }

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                jml_vm_push(frame->slots[slot]);
                break;
            }

            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = jml_vm_peek(0);
                break;
            }

            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                jml_vm_push(*frame->closure->upvalues[slot]->location);
                break;
            }

            case OP_CLOSE_UPVALUE: {
                jml_vm_upvalue_close(vm->stack_top - 1);
                jml_vm_pop();
                break;
            }

            case OP_SET_GLOBAL: {
                jml_obj_string_t *name = READ_STRING();
                if (jml_hashmap_set(&vm->globals, name, jml_vm_peek(0))) {
                    jml_hashmap_del(&vm->globals, name);
                    frame->pc = pc;
                    jml_vm_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_GET_GLOBAL: {
                jml_obj_string_t *name = READ_STRING();
                jml_value_t value;
                if (!jml_hashmap_get(&vm->globals, name, &value)) {
                    frame->pc = pc;
                    jml_vm_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                jml_vm_push(value);
                break;
            }

            case OP_DEF_GLOBAL: {
                jml_obj_string_t *name = READ_STRING();
                jml_hashmap_set(&vm->globals, name, jml_vm_peek(0));
                jml_vm_pop();
                break;
            }

            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(jml_vm_peek(1))) {
                    frame->pc = pc;
                    jml_vm_error("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_obj_instance_t *instance = AS_INSTANCE(jml_vm_peek(1));
                jml_hashmap_set(
                    &instance->fields, READ_STRING(), jml_vm_peek(0)
                );

                jml_value_t value = jml_vm_pop();
                jml_vm_pop();
                jml_vm_push(value);
                break;
            }

            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(jml_vm_peek(0))) {
                    frame->pc = pc;
                    jml_vm_error("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                jml_obj_instance_t *instance = AS_INSTANCE(jml_vm_peek(0));
                jml_obj_string_t *name = READ_STRING();

                jml_value_t value;
                if (jml_hashmap_get(&instance->fields, name, &value)) {
                    jml_vm_pop();
                    jml_vm_push(value);
                    break;
                }

                if (!jml_vm_method_bind(instance->klass, name))
                    return INTERPRET_RUNTIME_ERROR;

                break;
            }

            case OP_SUPER: {
                jml_obj_string_t *name = READ_STRING();
                jml_obj_class_t *superclass = AS_CLASS(jml_vm_pop());
                if (!jml_vm_method_bind(superclass, name)) {
                    frame->pc = pc;
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_ARRAY: {
                int item_count = READ_BYTE();

                jml_vm_push(
                    OBJ_VAL(jml_obj_array_new(vm->stack_top - item_count,
                        item_count))
                );
                break;
            }

            default:
                UNREACHABLE();
        }
    }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_STRING
#undef READ_CONST

#undef BINARY_OP
#undef BINARY_DIV
#undef BINARY_FN
}


void
jml_cfunction_register(const char *name,
    jml_cfunction function)
{
    jml_vm_push(OBJ_VAL(jml_obj_cfunction_new(name,
        function)));

    jml_hashmap_set(
        &vm->globals, AS_CFUNCTION(vm->stack[0])->name,
            vm->stack[0]
    );

    jml_vm_pop();
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

    return jml_vm_run();
}
