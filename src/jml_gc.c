#include <stdlib.h>
#include <stdio.h>

#include <jml.h>

#include <jml_gc.h>
#include <jml_vm.h>
#include <jml_module.h>
#include <jml_compiler.h>


#if defined JML_TRACE_GC || defined JML_ROUND_GC

#include <time.h>

#endif


void *
jml_reallocate(void *ptr,
    size_t old_size, size_t new_size)
{
    vm->allocated += new_size - old_size;

    if (new_size > old_size) {
#ifdef JML_STRESS_GC
        jml_gc_collect();
#else
        if (vm->allocated > vm->next_gc)
            jml_gc_collect();
#endif
    }

#ifdef JML_TRACE_MEM
    printf(
        "[MEM] |%p reallocated from %zd to %zd|\n",
        ptr, old_size, new_size
    );
#endif

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void *result = realloc(ptr, new_size);
    if (result == NULL) {
        printf("MemoryErr: %m\n");
        exit(EXIT_FAILURE);
    }

    return result;
}


void *
jml_realloc(void *ptr, size_t new_size)
{

#ifdef JML_TRACE_MEM
    printf(
        "[MEM] |%p reallocated to %zd|\n",
        ptr, new_size
    );
#endif

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void *result = realloc(ptr, new_size);
    if (result == NULL) {
        printf("MemoryErr: %m\n");
        exit(EXIT_FAILURE);
    }

    return result;
}


void *
jml_alloc(size_t size)
{
    void *ptr = jml_realloc(NULL, size);
    memset(ptr, 0, size);

    return ptr;
}


void
jml_free(void *ptr)
{
    if (ptr == NULL)
        return;

    jml_realloc(ptr, 0);
}


void
jml_gc_exempt(jml_value_t value)
{
    if (!IS_OBJ(value))
        return;

    jml_obj_t *object = AS_OBJ(value);
    if (object == NULL)
        return;

    object->exempt = true;
}


void
jml_gc_unexempt(jml_value_t value)
{
    if (!IS_OBJ(value))
        return;

    jml_obj_t *object = AS_OBJ(value);
    if (object == NULL)
        return;

    object->exempt = false;
}


static void
jml_gc_mark_roots(void)
{
    for (jml_value_t *slot = vm->stack;
        slot < vm->stack_top; ++slot) {

        jml_gc_mark_value(*slot);
    }

    for (int i = 0; i < vm->frame_count; ++i)
        jml_gc_mark_obj((jml_obj_t*)vm->frames[i].closure);

    for (jml_obj_upvalue_t *upvalue = vm->open_upvalues;
        upvalue != NULL; upvalue = upvalue->next) {

        jml_gc_mark_obj((jml_obj_t*)upvalue);
    }

    jml_hashmap_mark(&vm->globals);
    jml_hashmap_mark(&vm->modules);
    jml_hashmap_mark(&vm->builtins);

    for (jml_compiler_t **compiler = vm->compilers;
        compiler < vm->compiler_top; ++compiler) {

        jml_compiler_mark(*compiler);
    }

    jml_gc_mark_obj((jml_obj_t*)vm->main_string);
    jml_gc_mark_obj((jml_obj_t*)vm->module_string);
    jml_gc_mark_obj((jml_obj_t*)vm->path_string);
    jml_gc_mark_obj((jml_obj_t*)vm->init_string);
    jml_gc_mark_obj((jml_obj_t*)vm->call_string);
    jml_gc_mark_obj((jml_obj_t*)vm->free_string);
    jml_gc_mark_obj((jml_obj_t*)vm->add_string);
    jml_gc_mark_obj((jml_obj_t*)vm->sub_string);
    jml_gc_mark_obj((jml_obj_t*)vm->mul_string);
    jml_gc_mark_obj((jml_obj_t*)vm->pow_string);
    jml_gc_mark_obj((jml_obj_t*)vm->div_string);
    jml_gc_mark_obj((jml_obj_t*)vm->mod_string);
    jml_gc_mark_obj((jml_obj_t*)vm->gt_string);
    jml_gc_mark_obj((jml_obj_t*)vm->ge_string);
    jml_gc_mark_obj((jml_obj_t*)vm->lt_string);
    jml_gc_mark_obj((jml_obj_t*)vm->le_string);
    jml_gc_mark_obj((jml_obj_t*)vm->concat_string);
    jml_gc_mark_obj((jml_obj_t*)vm->get_string);
    jml_gc_mark_obj((jml_obj_t*)vm->set_string);
    jml_gc_mark_obj((jml_obj_t*)vm->size_string);
    jml_gc_mark_obj((jml_obj_t*)vm->print_string);
    jml_gc_mark_obj((jml_obj_t*)vm->string_string);

    jml_gc_mark_obj((jml_obj_t*)vm->current);
    jml_gc_mark_obj((jml_obj_t*)vm->external);

    jml_gc_mark_obj(vm->sentinel);
}


void
jml_gc_mark_obj(jml_obj_t *object)
{
    if (object == NULL || object->marked)
        return;

#ifdef JML_TRACE_GC
    printf("[GC]  |%p marked %s|\n",
        (void*)object,
        jml_obj_stringify_type(OBJ_VAL(object))
    );
#endif

    object->marked = true;
    if (vm->gray_capacity < vm->gray_count + 1) {
        vm->gray_capacity = GROW_CAPACITY(vm->gray_capacity);
        vm->gray_stack = (jml_obj_t**)jml_realloc(vm->gray_stack,
            sizeof(jml_obj_t*) * vm->gray_capacity);
    }
    vm->gray_stack[vm->gray_count++] = object;
}


void
jml_gc_mark_value(jml_value_t value)
{
    if (!IS_OBJ(value))
        return;

    jml_gc_mark_obj(AS_OBJ(value));
}


static void
jml_gc_mark_array(jml_value_array_t *array)
{
    for (int i = 0; i < array->count; ++i) {
        jml_gc_mark_value(array->values[i]);
    }
}


static void
jml_gc_free_object(jml_obj_t *object)
{
#ifdef JML_TRACE_MEM
    printf(
        "[MEM] |%p freed %s|\n",
        (void*)object,
        jml_obj_stringify_type(OBJ_VAL(object))
    );
#endif

    switch (object->type) {
        case OBJ_STRING: {
            jml_obj_string_t *string = (jml_obj_string_t*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(jml_obj_string_t, object);
            break;
        }

        case OBJ_ARRAY: {
            jml_value_array_free(&((jml_obj_array_t*)object)->values);
            FREE(jml_obj_array_t, object);
            break;
        }

        case OBJ_MAP: {
            jml_obj_map_t *map = (jml_obj_map_t*)object;
            jml_hashmap_free(&map->hashmap);
            FREE(jml_obj_map_t, object);
            break;
        }

        case OBJ_MODULE: {
            jml_obj_module_t *module = (jml_obj_module_t*)object;
            jml_module_finalize(module);
            jml_hashmap_free(&module->globals);
            FREE(jml_obj_module_t, object);
            break;
        }

        case OBJ_CLASS: {
            jml_obj_class_t *klass = (jml_obj_class_t*)object;
            jml_hashmap_free(&klass->methods);
            FREE(jml_obj_class_t, object);
            break;
        }

        case OBJ_INSTANCE: {
            jml_obj_instance_t *instance = (jml_obj_instance_t*)object;

            if (vm->free_string != NULL) {
                jml_value_t *destructor;
                if (jml_hashmap_get(&instance->klass->methods,
                    vm->free_string, &destructor)) {

                    if (IS_CFUNCTION(*destructor)) {
                        jml_vm_push(OBJ_VAL(*destructor));
                        jml_vm_push(OBJ_VAL(instance));

                        jml_vm_call_value(*destructor, 1);
                        jml_vm_pop();

                    } else {
                        /*jml_vm_call_cstack(*destructor, 0, NULL);*/
                    }
                }
            }

            instance->extra = NULL;
            jml_hashmap_free(&instance->fields);
            FREE(jml_obj_instance_t, object);
            break;
        }

        case OBJ_METHOD: {
            FREE(jml_obj_method_t, object);
            break;
        }

        case OBJ_FUNCTION: {
            jml_obj_function_t *function = (jml_obj_function_t*)object;
            jml_bytecode_free(&function->bytecode);
            FREE(jml_obj_function_t, object);
            break;
        }

        case OBJ_CLOSURE: {
            jml_obj_closure_t *closure = (jml_obj_closure_t*)object;
            FREE_ARRAY(jml_obj_upvalue_t*, closure->upvalues, closure->upvalue_count);
            FREE(jml_obj_closure_t, object);
            break;
        }

        case OBJ_UPVALUE: {
            FREE(jml_obj_upvalue_t, object);
            break;
        }

        case OBJ_CFUNCTION: {
            FREE(jml_obj_cfunction_t, object);
            break;
        }

        case OBJ_EXCEPTION: {
            FREE(jml_obj_exception_t, object);
            break;
        }
    }
}


void
jml_gc_free_objs(void)
{
    jml_obj_t *object           = vm->objects;

    while (object != NULL) {
        jml_obj_t *next         = object->next;

        if (object != vm->sentinel
            && object != (jml_obj_t*)vm->free_string)
            jml_gc_free_object(object);

        object                  = next;
    }

    jml_gc_free_object((jml_obj_t*)vm->free_string);

    jml_free(vm->sentinel);
    jml_free(vm->gray_stack);
}


static void
jml_gc_sweep(void)
{
    jml_obj_t *previous          = NULL;
    jml_obj_t *object            = vm->objects;

    while (object != NULL) {
        if (object->marked) {
            object->marked       = false;
            previous             = object;
            object               = object->next;

        } else if (object->exempt) {
            previous             = object;
            object               = object->next;

        } else {
            jml_obj_t *unreached = object;
            object               = object->next;

            if (previous != NULL)
                previous->next   = object;
            else
                vm->objects      = object;

            jml_gc_free_object(unreached);
        }
    }
}


static void
jml_gc_blacken_obj(jml_obj_t *object)
{
#ifdef JML_TRACE_GC
    printf("[GC]  |%p blackened %s|\n",
        (void*)object,
        jml_obj_stringify_type(OBJ_VAL(object))
    );
#endif
    switch (object->type) {
        case OBJ_STRING:
            break;

        case OBJ_ARRAY: {
            jml_gc_mark_array(&((jml_obj_array_t*)object)->values);
            break;
        }

        case OBJ_MAP: {
            jml_hashmap_mark(&((jml_obj_map_t*)object)->hashmap);
            break;
        }

        case OBJ_MODULE: {
            jml_obj_module_t *module = (jml_obj_module_t*)object;
            jml_gc_mark_obj((jml_obj_t*)module->name);
            jml_hashmap_mark(&module->globals);
            break;
        }

        case OBJ_INSTANCE: {
            jml_obj_instance_t *instance = (jml_obj_instance_t*)object;
            jml_gc_mark_obj((jml_obj_t*)instance->klass);
            jml_hashmap_mark(&instance->fields);
            break;
        }

        case OBJ_CLASS: {
            jml_obj_class_t *klass = (jml_obj_class_t*)object;
            jml_gc_mark_obj((jml_obj_t*)klass->name);
            jml_gc_mark_obj((jml_obj_t*)klass->super);
            jml_hashmap_mark(&klass->methods);
            break;
        }

        case OBJ_METHOD: {
            jml_obj_method_t *bound = (jml_obj_method_t*)object;
            jml_gc_mark_value(bound->receiver);
            jml_gc_mark_obj((jml_obj_t*)bound->method);
            break;
        }

        case OBJ_CLOSURE: {
            jml_obj_closure_t *closure = (jml_obj_closure_t*)object;
            jml_gc_mark_obj((jml_obj_t*)closure->function);
            for (int i = 0; i < closure->upvalue_count; ++i) {
                jml_gc_mark_obj((jml_obj_t*)closure->upvalues[i]);
            }
            break;
        }

        case OBJ_FUNCTION: {
            jml_obj_function_t *function = (jml_obj_function_t*)object;
            jml_gc_mark_obj((jml_obj_t*)function->name);
            jml_gc_mark_obj((jml_obj_t*)function->klass_name);
            jml_gc_mark_array(&function->bytecode.constants);
            break;
        }

        case OBJ_UPVALUE: {
            jml_gc_mark_value(((jml_obj_upvalue_t*)object)->closed);
            break;
        }

        case OBJ_CFUNCTION: {
            jml_obj_cfunction_t *cfunction = (jml_obj_cfunction_t*)object;
            jml_gc_mark_obj((jml_obj_t*)cfunction->name);
            jml_gc_mark_obj((jml_obj_t*)cfunction->klass_name);
            break;
        }

        case OBJ_EXCEPTION: {
            jml_obj_exception_t *exc = (jml_obj_exception_t*)object;
            jml_gc_mark_obj((jml_obj_t*)exc->name);
            jml_gc_mark_obj((jml_obj_t*)exc->message);
            break;
        }
    }
}


static void
jml_gc_trace_refs(void)
{
    while (vm->gray_count > 0) {
        jml_obj_t *object = vm->gray_stack[--vm->gray_count];
        jml_gc_blacken_obj(object);
    }
}


void
jml_gc_collect(void)
{
#ifdef JML_ROUND_GC
    static int generation = 0;

    size_t before = vm->allocated;
    time_t start  = clock();

    printf(
        "[GC]  |gc started {current: %zd bytes, generation: %d}|\n",
        before, generation
    );
#endif

    jml_gc_mark_roots();
    jml_gc_trace_refs();
    jml_hashmap_remove_white(&vm->strings);
    jml_gc_sweep();

    vm->next_gc = vm->allocated * GC_HEAP_GROW_FACTOR;

#ifdef JML_ROUND_GC
    time_t elapsed = clock() - start;
    size_t after = vm->allocated;

    printf(
        "[GC]  |gc ended {current: %zd bytes, collected: %zd bytes, next: %zd bytes, elapsed: %.3lds}|\n",
        after, before - after, vm->next_gc, (long)elapsed
    );

    ++generation;
#endif
}
