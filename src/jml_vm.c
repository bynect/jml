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
#include <jml_util.h>


jml_vm_t *vm;


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
    jml_vm_reset(vm);

    jml_obj_t *sentinel     = jml_alloc(sizeof(jml_obj_t));

    vm->sentinel            = sentinel;
    vm->objects             = vm->sentinel;
    vm->external            = NULL;

    vm->allocated           = 0;
    vm->next_gc             = 1024 * 1024 * 4;

    vm->gray_count          = 0;
    vm->gray_capacity       = 0;
    vm->gray_stack          = NULL;

    jml_hashmap_init(&vm->globals);
    jml_hashmap_init(&vm->strings);
    jml_hashmap_init(&vm->modules);

    vm->main_string         = NULL;
    vm->init_string         = NULL;
    vm->call_string         = NULL;
    vm->free_string         = NULL;
    vm->get_string          = NULL;
    vm->set_string          = NULL;
    vm->module_string       = NULL;
    vm->path_string         = NULL;

    vm->main_string         = jml_obj_string_copy("__main", 6);
    vm->init_string         = jml_obj_string_copy("__init", 6);
    vm->call_string         = jml_obj_string_copy("__call", 6);
    vm->free_string         = jml_obj_string_copy("__free", 6);
    vm->get_string          = jml_obj_string_copy("__get", 5);
    vm->set_string          = jml_obj_string_copy("__set", 5);
    vm->module_string       = jml_obj_string_copy("__module", 8);
    vm->path_string         = jml_obj_string_copy("__path", 6);

    jml_core_register();
}


void
jml_vm_free(jml_vm_t *vm)
{
    jml_hashmap_free(&vm->globals);
    jml_hashmap_free(&vm->strings);
    jml_hashmap_free(&vm->modules);

    vm->external            = NULL;

    jml_gc_free_objs();

    vm->main_string         = NULL;
    vm->init_string         = NULL;
    vm->call_string         = NULL;
    vm->free_string         = NULL;
    vm->get_string          = NULL;
    vm->set_string          = NULL;
    vm->module_string       = NULL;
    vm->path_string         = NULL;

    JML_ASSERT(
        vm->allocated == 0,
        "[VM]  |%zd bytes not freed|\n",
        vm->allocated
    );

    jml_free(vm);
}


void
jml_vm_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);
    fputs("\n", stderr);

    va_end(args);

#ifndef JML_BACKTRACE
    if (vm->external != NULL) {
        jml_call_frame_t    *frame    = &vm->frames[0];
        jml_obj_function_t  *function = frame->closure->function;

        size_t instruction = frame->pc - function->bytecode.code - 1;

        fprintf(stderr, "[line %d] in function ",
            function->bytecode.lines[instruction]
        );

        if (vm->external->module != NULL)
            fprintf(stderr, "%s.", vm->external->module->name->chars);

        if (vm->external->klass_name != NULL)
            fprintf(stderr, "%s.", vm->external->klass_name->chars);

        fprintf(stderr, "%s\n", vm->external->name->chars);
    }

    for (int i = vm->frame_count - 1; i >= 0; --i) {
#else
    for (int i = 0; i < vm->frame_count; ++i) {
#endif
        jml_call_frame_t    *frame    = &vm->frames[i];
        jml_obj_function_t  *function = frame->closure->function;

        size_t instruction = frame->pc - function->bytecode.code - 1;

        fprintf(stderr, "[line %d] in function ",
            function->bytecode.lines[instruction]
        );

        if (function->name != NULL) {
            if (function->module != NULL)
                fprintf(stderr, "%s.", function->module->name->chars);

            if (function->klass_name != NULL)
                fprintf(stderr, "%s.", function->klass_name->chars);

            fprintf(stderr, "%s/%d\n", function->name->chars,
                function->arity);
        } else
            fprintf(stderr, "__main\n");
    }

#ifdef JML_BACKTRACE
    if (vm->external != NULL) {
        jml_call_frame_t    *frame    = &vm->frames[vm->frame_count - 1];
        jml_obj_function_t  *function = frame->closure->function;

        size_t instruction = frame->pc - function->bytecode.code - 1;

        fprintf(stderr, "[line %d] in function ",
            function->bytecode.lines[instruction]
        );

        if (vm->external->module != NULL)
            fprintf(stderr, "%s.", vm->external->module->name->chars);

        if (vm->external->klass_name != NULL)
            fprintf(stderr, "%s.", vm->external->klass_name->chars);

        fprintf(stderr, "%s\n", vm->external->name->chars);
    }
#endif

    jml_vm_reset(vm);
}


static void
jml_vm_exception(jml_obj_exception_t *exc)
{
    jml_vm_error(
        "%.*s: %.*s",
        (int)exc->name->length, exc->name->chars,
        (int)exc->message->length, exc->message->chars
    );
}


void
jml_vm_reset(jml_vm_t *vm)
{
    vm->stack_top       = vm->stack;
    vm->frame_count     = 0;
    vm->open_upvalues   = NULL;
    vm->external        = NULL;
}


void
jml_vm_push(jml_value_t value)
{
    *vm->stack_top = value;
    ++vm->stack_top;
}


jml_value_t
jml_vm_pop(void)
{
    --vm->stack_top;
    return *vm->stack_top;
}


void
jml_vm_pop_two(void)
{
    vm->stack_top -= 2;
}


void
jml_vm_rot(void)
{
    jml_value_t a       = jml_vm_pop();
    jml_value_t b       = jml_vm_pop();
    jml_vm_push(a);
    jml_vm_push(b);
}


jml_value_t
jml_vm_peek(int distance)
{
    return vm->stack_top[-1 - distance];
}


static inline bool
jml_vm_global_get(jml_obj_string_t *name,
    jml_value_t *value)
{
    return jml_hashmap_get(&vm->globals, name, value);
}


static inline bool
jml_vm_global_set(jml_obj_string_t *name,
    jml_value_t value)
{
    return jml_hashmap_set(&vm->globals, name, value);
}


static inline bool
jml_vm_global_del(jml_obj_string_t *name)
{
    return jml_hashmap_del(&vm->globals, name);
}


static inline bool
jml_vm_global_pop(jml_obj_string_t *name,
    jml_value_t *value)
{
    return jml_hashmap_pop(&vm->globals, name, value);
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


bool
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
                jml_obj_class_t    *klass       = AS_CLASS(callee);
                jml_value_t         instance    = OBJ_VAL(
                    jml_obj_instance_new(klass));

                vm->stack_top[-arg_count - 1]   = instance;
                jml_value_t initializer;

                if (jml_hashmap_get(&klass->methods,
                    vm->init_string, &initializer)) {

                    if (IS_CFUNCTION(initializer)) {
                        bool result = jml_vm_call_value(initializer, arg_count + 1);
                        jml_vm_push(instance);
                        return result;

                    } else
                        return jml_vm_call(AS_CLOSURE(initializer), arg_count);

                } else if (arg_count != 0) {
                    jml_vm_error(
                        "Expected '0' arguments but got %d.", arg_count
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

                    if (IS_CFUNCTION(caller)) {
                        jml_vm_push(callee);
                        return jml_vm_call_value(caller, arg_count + 1);
                    } else
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
                jml_value_t          result         = cfunction(arg_count,
                    vm->stack_top - arg_count);

                vm->stack_top -= arg_count + 1;

                if (IS_EXCEPTION(result)) {
                    vm->external = cfunction_obj;
                    jml_vm_exception(AS_EXCEPTION(result));
                    return false;

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
        "Can call only functions, classes and instances."
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

    if (IS_CFUNCTION(method))
        return jml_vm_call_value(method, arg_count + 1);
    else
        return jml_vm_call(AS_CLOSURE(method), arg_count);
}


static bool
jml_vm_invoke_instance(jml_obj_instance_t *instance,
    jml_obj_string_t *name, int arg_count)
{
    jml_value_t method;

    if (!jml_hashmap_get(&instance->klass->methods, name, &method)) {
        jml_vm_error("Undefined property '%s'.", name->chars);
        return false;
    }

    if (IS_CFUNCTION(method)) {
        jml_vm_push(OBJ_VAL(instance));
        return jml_vm_call_value(method, arg_count + 1);

    } else
        return jml_vm_call(AS_CLOSURE(method), arg_count);
}


static bool
jml_vm_invoke(jml_obj_string_t *name, int arg_count)
{
    jml_value_t receiver = jml_vm_peek(arg_count);

    if (IS_INSTANCE(receiver)) {
        jml_obj_instance_t *instance = AS_INSTANCE(receiver);
        jml_value_t         value;

        if (jml_hashmap_get(&instance->fields, name, &value)) {
            vm->stack_top[-arg_count - 1] = value;
            return jml_vm_call_value(value, arg_count);
        }

        return jml_vm_invoke_instance(
            instance, name, arg_count);

    } else if (IS_MODULE(receiver)) {
        jml_obj_module_t *module = AS_MODULE(receiver);
        jml_value_t value;

        if (jml_hashmap_get(&module->globals, name, &value))
            return jml_vm_call_value(value, arg_count);

#ifndef JML_LAZY_IMPORT
        else {
            jml_vm_error("Undefined property '%s'.", name->chars);
            return false;
        }
#else
        else if (module->handle != NULL) {
            jml_obj_cfunction_t *cfunction = jml_module_get_raw(
                module, name->chars, true);

            if (cfunction == NULL || cfunction->function == NULL) {
                jml_vm_error("Undefined property '%s'.", name->chars);
                return false;
            }

            value = OBJ_VAL(cfunction);
            jml_vm_push(value);
            jml_hashmap_set(&module->globals, name, jml_vm_peek(0));
            jml_vm_pop();
            return jml_vm_call_value(value, arg_count);
        }
#endif
    }

    jml_vm_error("Can't call '%s'.", name->chars);
    return false;
}


static bool
jml_vm_method_bind(jml_obj_class_t *klass,
    jml_obj_string_t *name)
{
    jml_value_t method;
    if (!jml_hashmap_get(&klass->methods, name, &method)) {
        jml_vm_error(
            "Undefined property '%s'.", name->chars
        );
        return false;
    }

    jml_value_t bound;
    if (IS_CFUNCTION(method))
        bound = OBJ_VAL(AS_CFUNCTION(method));
    else
        bound = OBJ_VAL(jml_obj_method_new(
            jml_vm_peek(0), AS_CLOSURE(method)
        ));

    jml_vm_pop();
    jml_vm_push(bound);
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
    while (vm->open_upvalues != NULL
        && vm->open_upvalues->location >= last) {

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


static bool
jml_string_equal(jml_obj_string_t *string1,
    jml_obj_string_t *string2)
{
    return (string1->length == string2->length)
        && (strncmp(string1->chars, string2->chars,
            string1->length) == 0);
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
jml_vm_module_import(jml_obj_string_t *name, bool global)
{
    jml_value_t value;

    if (!jml_hashmap_get(&vm->modules, name, &value)) {
        char path[JML_PATH_MAX];

        jml_obj_module_t *module = jml_module_open(name, path);
        if (module == NULL)
            return false;

        jml_vm_push(OBJ_VAL(module));

        if (path != NULL) {
            jml_vm_push(jml_string_intern(path));

            jml_hashmap_set(&module->globals,
                vm->path_string, jml_vm_peek(0));
            jml_vm_pop();
        } else {
            jml_hashmap_set(&module->globals,
                vm->path_string, NONE_VAL);
        }

        jml_hashmap_set(&module->globals,
            vm->module_string, OBJ_VAL(module->name));

        if (!jml_module_initialize(module)) {
            jml_vm_error("ImportErr: Import of '%s' failed.",
                module->name->chars);
            return false;
        }

        jml_hashmap_set(&vm->modules, name, jml_vm_peek(0));

        if (global)
            jml_vm_global_set(name, jml_vm_peek(0));

    } else {
        jml_vm_push(value);

        if (global)
            jml_vm_global_set(name, value);
    }

    return true;
}


static bool
jml_vm_module_bind(jml_obj_module_t *module,
    jml_obj_string_t *name)
{
    jml_value_t function;
    if (!jml_hashmap_get(&module->globals, name, &function)) {
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
                "Undefined member '%s'.", name->chars
            );
            return false;
        }
    }

    jml_vm_pop();
    jml_vm_push(function);
    return true;
}


static jml_interpret_result
jml_vm_run(jml_value_t *last)
{
    register jml_call_frame_t *frame = &vm->frames[vm->frame_count - 1];
    register uint8_t *pc = frame->pc;

#ifndef JML_EVAL
    (void) last;
#endif


#define SAVE_FRAME()                (frame->pc = pc)
#define LOAD_FRAME()                                    \
    do {                                                \
        frame = &vm->frames[vm->frame_count - 1];       \
        pc = frame->pc;                                 \
    } while (false)


#define READ_BYTE()                 (*pc++)
#define READ_SHORT()                (pc += 2, (uint16_t)((pc[-2] << 8) | pc[-1]))
#define READ_STRING()               AS_STRING(READ_CONST())
#define READ_CSTRING()              AS_CSTRING(READ_CONST())
#define READ_CONST()                                    \
    (frame->closure->function->bytecode.constants.values[READ_BYTE()])


#define BINARY_OP(type, op, num_type)                   \
    do {                                                \
        if (!IS_NUM(jml_vm_peek(0))                     \
            || !IS_NUM(jml_vm_peek(1))) {               \
            SAVE_FRAME();                               \
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
            SAVE_FRAME();                               \
            jml_vm_error(                               \
                "Operands must be numbers."             \
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        num_type b = AS_NUM(jml_vm_pop());              \
        num_type a = AS_NUM(jml_vm_pop());              \
        if (b == 0) {                                   \
            SAVE_FRAME();                               \
            jml_vm_error(                               \
                "Can't divide by zero."                 \
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        jml_vm_push(type(a op b));                      \
    } while (false)


#define BINARY_FN(type, fn, num_type)                   \
    do {                                                \
        if (!IS_NUM(jml_vm_peek(0))                     \
            || !IS_NUM(jml_vm_peek(1))) {               \
            SAVE_FRAME();                               \
            jml_vm_error(                               \
                "Operands must be numbers."             \
            );                                          \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        num_type b = AS_NUM(jml_vm_pop());              \
        num_type a = AS_NUM(jml_vm_pop());              \
        jml_vm_push(type(fn(a, b)));                    \
    } while (false)


#ifdef JML_COMPUTED_GOTO

#define DISPATCH()                  goto *dispatcher[READ_BYTE()]
#define EXEC_OP(op)                 exec_##op:

#ifdef JML_TRACE_STACK
#define END_OP()                    goto trace_stack
#else
#define END_OP()                    DISPATCH()
#endif

    static void *dispatcher[] = {
        &&exec_OP_POP,
        &&exec_OP_POP_TWO,
        &&exec_OP_ROT,
        &&exec_OP_SAVE,
        &&exec_OP_CONST,
        &&exec_OP_NONE,
        &&exec_OP_TRUE,
        &&exec_OP_FALSE,
        &&exec_OP_ADD,
        &&exec_OP_SUB,
        &&exec_OP_MUL,
        &&exec_OP_DIV,
        &&exec_OP_POW,
        &&exec_OP_MOD,
        &&exec_OP_NOT,
        &&exec_OP_NEGATE,
        &&exec_OP_EQUAL,
        &&exec_OP_GREATER,
        &&exec_OP_GREATEREQ,
        &&exec_OP_LESS,
        &&exec_OP_LESSEQ,
        &&exec_OP_NOTEQ,
        &&exec_OP_CONTAIN,
        &&exec_OP_JUMP,
        &&exec_OP_JUMP_IF_FALSE,
        &&exec_OP_LOOP,
        &&exec_OP_CALL,
        &&exec_OP_METHOD,
        &&exec_OP_INVOKE,
        &&exec_OP_SUPER_INVOKE,
        &&exec_OP_CLOSURE,
        &&exec_OP_RETURN,
        &&exec_OP_CLASS,
        &&exec_OP_INHERIT,
        &&exec_OP_SET_LOCAL,
        &&exec_OP_GET_LOCAL,
        &&exec_OP_SET_UPVALUE,
        &&exec_OP_GET_UPVALUE,
        &&exec_OP_CLOSE_UPVALUE,
        &&exec_OP_SET_GLOBAL,
        &&exec_OP_GET_GLOBAL,
        &&exec_OP_DEF_GLOBAL,
        &&exec_OP_SET_MEMBER,
        &&exec_OP_GET_MEMBER,
        &&exec_OP_SET_INDEX,
        &&exec_OP_GET_INDEX,
        &&exec_OP_SWAP_GLOBAL,
        &&exec_OP_SWAP_LOCAL,
        &&exec_OP_SUPER,
        &&exec_OP_ARRAY,
        &&exec_OP_MAP,
        &&exec_OP_IMPORT_GLOBAL,
        &&exec_OP_IMPORT_LOCAL,
        &&exec_OP_IMPORT_WILDCARD
    };

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
        for (jml_value_t *slot = vm->stack; slot < vm->stack_top; ++slot) {
            printf("[ ");
            jml_value_print(*slot);
            printf(" ]");
        }
        printf("\n");

#ifdef JML_STEP_STACK
        jml_bytecode_instruction_disassemble(&frame->closure->function->bytecode,
            (int)(frame->pc - frame->closure->function->bytecode.code));
#endif

#ifdef JML_COMPUTED_GOTO
        DISPATCH();
    }
#endif
#endif

#ifndef JML_COMPUTED_GOTO
        switch (instruction = READ_BYTE()) {
#endif

            EXEC_OP(OP_POP) {
#ifdef JML_EVAL
                jml_value_t value = jml_vm_pop();
                if (vm->frame_count - 1 == 0) {
                    if (last != NULL)
                        *last = value;
                }
#else
                jml_vm_pop();
#endif
                END_OP();
            }

            EXEC_OP(OP_POP_TWO) {
                jml_vm_pop_two();
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

            EXEC_OP(OP_ADD) {
                if (IS_STRING(jml_vm_peek(1))) {
                    if (IS_STRING(jml_vm_peek(0))) {
                        jml_string_concatenate();

                    } else if (IS_NUM(jml_vm_peek(0))) {
                        char *temp = jml_value_stringify(jml_vm_pop());
                        jml_vm_push(OBJ_VAL(
                            jml_obj_string_take(temp, strlen(temp))
                        ));

                        jml_string_concatenate();

                    } else {
                        SAVE_FRAME();
                        jml_vm_error(
                            "Can't concatenate string to %s.",
                            jml_value_stringify_type(jml_vm_peek(0))
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                } else if (IS_NUM(jml_vm_peek(0))
                        && IS_NUM(jml_vm_peek(1))) {

                    BINARY_OP(NUM_VAL, +, double);

                } else if (IS_ARRAY(jml_vm_peek(1))) {

                    jml_array_concatenate();

                } else {
                    SAVE_FRAME();
                    jml_vm_error(
                        "Operands must be numbers, strings or array."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_SUB) {
                BINARY_OP(NUM_VAL, -, double);
                END_OP();
            }

            EXEC_OP(OP_MUL) {
                BINARY_OP(NUM_VAL, *, double);
                END_OP();
            }

            EXEC_OP(OP_DIV) {
                BINARY_DIV(NUM_VAL, /, double);
                END_OP();
            }

            EXEC_OP(OP_MOD) {
                BINARY_DIV(NUM_VAL, %, int);
                END_OP();
            }

            EXEC_OP(OP_POW) {
                BINARY_FN(NUM_VAL, pow, double);
                END_OP();
            }

            EXEC_OP(OP_NOT) {
                jml_vm_push(
                    BOOL_VAL(jml_is_falsey(jml_vm_pop()))
                );
                END_OP();
            }

            EXEC_OP(OP_NEGATE) {
                if (!IS_NUM(jml_vm_peek(0))) {
                    SAVE_FRAME();
                    jml_vm_error(
                        "Operand must be a number."
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
                BINARY_OP(BOOL_VAL, >, double);
                END_OP();
            }

            EXEC_OP(OP_GREATEREQ) {
                BINARY_OP(BOOL_VAL, >=, double);
                END_OP();
            }

            EXEC_OP(OP_LESS) {
                BINARY_OP(BOOL_VAL, <, double);
                END_OP();
            }

            EXEC_OP(OP_LESSEQ) {
                BINARY_OP(BOOL_VAL, <=, double);
                END_OP();
            }

            EXEC_OP(OP_NOTEQ) {
                jml_value_t b = jml_vm_pop();
                jml_value_t a = jml_vm_pop();
                jml_vm_push(
                    BOOL_VAL(!jml_value_equal(a, b)));

                END_OP();
            }

            EXEC_OP(OP_CONTAIN) {
                jml_value_t box     = jml_vm_pop();
                jml_value_t value   = jml_vm_pop();

                if (IS_ARRAY(box)) {
                    jml_obj_array_t *array = AS_ARRAY(box);
                    for (int i = 0; i < array->values.count; ++i) {
                        if (jml_value_equal(value, array->values.values[i])) {
                            jml_vm_push(TRUE_VAL);
                            END_OP();
                        }
                    }
                } else {
                    SAVE_FRAME();
                    jml_vm_error("Container must be iterable.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_push(FALSE_VAL);
                END_OP();
            }

            EXEC_OP(OP_JUMP) {
                uint16_t offset = READ_SHORT();
                pc += offset;
                END_OP();
            }

            EXEC_OP(OP_JUMP_IF_FALSE) {
                uint16_t offset = READ_SHORT();
                if (jml_is_falsey(jml_vm_peek(0)))
                    pc += offset;
                END_OP();
            }

            EXEC_OP(OP_LOOP) {
                uint16_t offset = READ_SHORT();
                pc -= offset;
                END_OP();
            }

            EXEC_OP(OP_CALL) {
                int             arg_count = READ_BYTE();
                SAVE_FRAME();
                if (!jml_vm_call_value(jml_vm_peek(arg_count), arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_METHOD) {
                jml_vm_method_define(READ_STRING());
                END_OP();
            }

            EXEC_OP(OP_INVOKE) {
                jml_obj_string_t *name      = READ_STRING();
                int               arg_count = READ_BYTE();

                SAVE_FRAME();
                if (!jml_vm_invoke(name, arg_count))
                    return INTERPRET_RUNTIME_ERROR;

                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_SUPER_INVOKE) {
                jml_obj_string_t *method    = READ_STRING();
                int               arg_count = READ_BYTE();

                SAVE_FRAME();
                jml_obj_class_t *superclass = AS_CLASS(jml_vm_pop());
                if (!jml_vm_invoke_class(superclass, method, arg_count))
                    return INTERPRET_RUNTIME_ERROR;

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
                        closure->upvalues[i] = jml_vm_upvalue_capture(frame->slots + index);
                    else
                        closure->upvalues[i] = frame->closure->upvalues[index];
                }
                END_OP();
            }

            EXEC_OP(OP_RETURN) {
                jml_value_t result = jml_vm_pop();
                jml_vm_upvalue_close(frame->slots);
                --vm->frame_count;

                if (vm->frame_count == 0) {
                    jml_vm_pop();
                    return INTERPRET_OK;
                }

                vm->stack_top = frame->slots;
                jml_vm_push(result);
                LOAD_FRAME();
                END_OP();
            }

            EXEC_OP(OP_CLASS) {
                jml_vm_push(
                    OBJ_VAL(jml_obj_class_new(READ_STRING()))
                );
                END_OP();
            }

            EXEC_OP(OP_INHERIT) {
                jml_value_t superclass = jml_vm_peek(1);
                if (!IS_CLASS(superclass)) {
                    SAVE_FRAME();
                    jml_vm_error("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!AS_CLASS(superclass)->inheritable) {
                    SAVE_FRAME();
                    jml_vm_error("Superclass must be inheritable.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_obj_class_t *subclass   = AS_CLASS(jml_vm_peek(0));
                subclass->super             = AS_CLASS(superclass);

                jml_hashmap_add(
                    &AS_CLASS(superclass)->methods,
                    &subclass->methods
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

            EXEC_OP(OP_GET_LOCAL) {
                uint8_t slot = READ_BYTE();

                printf("\n %d \n", slot);
                jml_value_print(frame->slots[slot]);
                printf("\n");

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
                *frame->closure->upvalues[slot]->location = jml_vm_peek(0);
                END_OP();
            }

            EXEC_OP(OP_GET_UPVALUE) {
                uint8_t slot = READ_BYTE();
                jml_vm_push(*frame->closure->upvalues[slot]->location);
                END_OP();
            }

            EXEC_OP(OP_CLOSE_UPVALUE) {
                jml_vm_upvalue_close(vm->stack_top - 1);
                jml_vm_pop();
                END_OP();
            }

            EXEC_OP(OP_SET_GLOBAL) {
                jml_obj_string_t *name = READ_STRING();
                if (jml_vm_global_set(name, jml_vm_peek(0))) {
                    jml_vm_global_del(name);
                    SAVE_FRAME();
                    jml_vm_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_GET_GLOBAL) {
                jml_obj_string_t *name = READ_STRING();
                jml_value_t value;
                if (!jml_vm_global_get(name, &value)) {
                    SAVE_FRAME();
                    jml_vm_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                jml_vm_push(value);
                END_OP();
            }

            EXEC_OP(OP_DEF_GLOBAL) {
                jml_obj_string_t *name = READ_STRING();
                jml_vm_global_set(name, jml_vm_peek(0));
                jml_vm_pop();
                END_OP();
            }

            EXEC_OP(OP_SET_MEMBER) {
                jml_value_t             peeked      = jml_vm_peek(1);

                if (IS_INSTANCE(peeked)) {
                    jml_obj_instance_t *instance    = AS_INSTANCE(peeked);
                    jml_hashmap_set(
                        &instance->fields, READ_STRING(), jml_vm_peek(0)
                    );

                    jml_value_t value = jml_vm_pop();
                    jml_vm_pop();
                    jml_vm_push(value);
                    END_OP();

                } else if (IS_MODULE(peeked)) {
                    jml_obj_module_t   *module      = AS_MODULE(peeked);
                    jml_hashmap_set(
                        &module->globals, READ_STRING(), jml_vm_peek(0)
                    );

                    jml_value_t value = jml_vm_pop();
                    jml_vm_pop();
                    jml_vm_push(value);
                    END_OP();

                } else {
                    SAVE_FRAME();
                    jml_vm_error(
                        "Only instances and modules have properties."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_GET_MEMBER) {
                jml_value_t             peeked      = jml_vm_peek(0);

                if (IS_INSTANCE(peeked)) {
                    jml_obj_instance_t *instance    = AS_INSTANCE(peeked);
                    jml_obj_string_t   *name        = READ_STRING();

                    jml_value_t value;
                    if (jml_hashmap_get(&instance->fields, name, &value)) {
                        jml_vm_pop();
                        jml_vm_push(value);
                        END_OP();
                    }

                    SAVE_FRAME();
                    if (!jml_vm_method_bind(instance->klass, name))
                        return INTERPRET_RUNTIME_ERROR;

                } else if (IS_MODULE(peeked)) {
                    jml_obj_module_t   *module      = AS_MODULE(peeked);
                    jml_obj_string_t   *name        = READ_STRING();

                    jml_value_t value;
                    if (jml_hashmap_get(&module->globals, name, &value)) {
                        jml_vm_pop();
                        jml_vm_push(value);
                        END_OP();
                    }

                    SAVE_FRAME();
                    if (!jml_vm_module_bind(module, name))
                        return INTERPRET_RUNTIME_ERROR;

                } else {
                    SAVE_FRAME();
                    jml_vm_error(
                        "Only instances and modules have properties."
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
                        jml_vm_error("Maps can be indexed only by strings.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_hashmap_set(
                        &AS_MAP(box)->hashmap, AS_STRING(index), value
                    );

                } else if (IS_ARRAY(box)) {
                    if (!IS_NUM(index)) {
                        SAVE_FRAME();
                        jml_vm_error("Arrays can be indexed only by numbers.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_value_array_t array = AS_ARRAY(box)->values;
                    int num_index   = AS_NUM(index);

                    if (num_index >= array.count || num_index < -array.count) {
                        SAVE_FRAME();
                        jml_vm_error("Out of bounds assignment to array.");
                        return INTERPRET_RUNTIME_ERROR;

                    }

                    if (num_index < 0)
                        array.values[array.count + num_index]   = value;
                    else
                        array.values[num_index]                 = value;

                } else if (IS_INSTANCE(box)) {
                    SAVE_FRAME();
                    if (!jml_vm_invoke_instance(AS_INSTANCE(box), vm->set_string, 2)) {
                        jml_vm_error(
                            "Instance of class '%s' is not indexable.",
                            AS_INSTANCE(box)->klass->name->chars
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    LOAD_FRAME();
                    END_OP();

                } else {
                    SAVE_FRAME();
                    jml_vm_error("Can index only arrays, maps and instances.");
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
                        jml_vm_error("Maps can be indexed only by strings.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    if (!jml_hashmap_get(&AS_MAP(box)->hashmap,
                        AS_STRING(index), &value))
                        value       = NONE_VAL;

                } else if (IS_ARRAY(box)) {
                    if (!IS_NUM(index)) {
                        SAVE_FRAME();
                        jml_vm_error("Arrays can be indexed only by numbers.");
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
                    if (!jml_vm_invoke_instance(AS_INSTANCE(box), vm->get_string, 1)) {
                        jml_vm_error(
                            "Instance of class '%s' is not indexable.",
                            AS_INSTANCE(box)->klass->name->chars
                        );
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    LOAD_FRAME();
                    END_OP();

                } else {
                    SAVE_FRAME();
                    jml_vm_error("Can index only arrays, maps and instances.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_pop_two();
                jml_vm_push(value);
                END_OP();
            }

            EXEC_OP(OP_SWAP_GLOBAL) {
                jml_obj_string_t *old_name  = READ_STRING();
                jml_obj_string_t *new_name  = READ_STRING();

                jml_value_t value;
                if (!jml_vm_global_pop(old_name, &value)) {
                    SAVE_FRAME();
                    jml_vm_error("Undefined variable '%s'.", old_name->chars);
                    return INTERPRET_RUNTIME_ERROR;

                } else if (jml_string_equal(old_name, new_name)) {
                    jml_vm_global_set(old_name, value);
                    SAVE_FRAME();
                    jml_vm_error("Can't swap variable to itself.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_global_set(new_name, value);
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
                if (!jml_vm_method_bind(superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                END_OP();
            }

            EXEC_OP(OP_ARRAY) {
                uint8_t          item_count  = READ_BYTE();
                jml_value_t     *values      = vm->stack_top -= item_count;
                jml_obj_array_t *array       = jml_obj_array_new();
                jml_value_t      array_value = OBJ_VAL(array);
                jml_gc_exempt(array_value);

                for (int i = 0; i < item_count; ++i) {
                    jml_obj_array_append(
                        array, values[i]
                    );
                }

                jml_gc_unexempt(array_value);
                jml_vm_push(array_value);
                END_OP();
            }

            EXEC_OP(OP_MAP) {
                uint8_t          item_count  = READ_BYTE();
                jml_value_t     *values      = vm->stack_top -= item_count;
                jml_obj_map_t   *map         = jml_obj_map_new();
                jml_value_t      map_value   = OBJ_VAL(map);
                jml_gc_exempt(map_value);

                for (int i = 0; i < item_count; i += 2) {
                    if (!IS_STRING(values[i])) {
                        SAVE_FRAME();
                        jml_vm_error("Map key must be a string.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    jml_hashmap_set(
                        &map->hashmap,
                        AS_STRING(values[i]),
                        values[i + 1]
                    );
                }

                jml_gc_unexempt(map_value);
                jml_vm_push(map_value);
                END_OP();
            }

            EXEC_OP(OP_IMPORT_GLOBAL) {
                SAVE_FRAME();
                if (!jml_vm_module_import(READ_STRING(), true)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_vm_pop();
                END_OP();
            }

            EXEC_OP(OP_IMPORT_LOCAL) {
                SAVE_FRAME();
                if (!jml_vm_module_import(READ_STRING(), false)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                uint8_t slot = READ_BYTE();
                frame->slots[slot] = jml_vm_pop();
                END_OP();
            }

            EXEC_OP(OP_IMPORT_WILDCARD) {
                SAVE_FRAME();
                if (!jml_vm_module_import(READ_STRING(), false)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                jml_hashmap_add(
                    &AS_MODULE(jml_vm_peek(0))->globals, &vm->globals
                );

                jml_vm_pop();
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
#undef READ_STRING
#undef READ_CSTRING
#undef READ_CONST

#undef BINARY_OP
#undef BINARY_DIV
#undef BINARY_FN

#undef EXEC_OP
#undef END_OP
}


void
jml_cfunction_register(const char *name,
    jml_cfunction function, jml_obj_module_t *module)
{
    jml_vm_push(jml_string_intern(name));

    jml_vm_push(OBJ_VAL(
        jml_obj_cfunction_new(AS_STRING(jml_vm_peek(0)),
            function, module)
    ));

    jml_vm_global_set(
        AS_STRING(jml_vm_peek(1)),
        jml_vm_peek(0)
    );

    jml_vm_pop_two();
}


jml_interpret_result
jml_vm_interpret(const char *source)
{
    jml_obj_function_t *function = jml_compiler_compile(
        source, NULL, true);

    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    jml_vm_push(OBJ_VAL(function));
    jml_obj_closure_t *closure = jml_obj_closure_new(function);

    jml_vm_pop();
    jml_vm_push(OBJ_VAL(closure));

    jml_vm_global_set(vm->module_string, OBJ_VAL(vm->main_string));
    jml_vm_global_set(vm->path_string, NONE_VAL);

    jml_vm_call_value(OBJ_VAL(closure), 0);
    return jml_vm_run(NULL);
}


jml_value_t jml_vm_eval(const char *source)
{
#ifdef JML_EVAL
    jml_obj_function_t *function = jml_compiler_compile(
        source, NULL, true);

    if (function == NULL)
        goto err;

    jml_vm_push(OBJ_VAL(function));
    jml_obj_closure_t *closure = jml_obj_closure_new(function);

    jml_vm_pop();
    jml_vm_push(OBJ_VAL(closure));

    jml_vm_global_set(vm->module_string, OBJ_VAL(vm->main_string));
    jml_vm_global_set(vm->path_string, NONE_VAL);

    jml_vm_call_value(OBJ_VAL(closure), 0);

    jml_value_t result = NONE_VAL;
    if (jml_vm_run(&result) != INTERPRET_OK)
        goto err;

    return result;
#else
    (void) source;
    goto err;
#endif

err:
    return OBJ_VAL(vm->sentinel);
}
