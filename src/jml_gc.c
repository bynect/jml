#include <stdlib.h>

#include <jml_gc.h>
#include <jml_vm.h>

#ifdef JML_TRACE_GC
#include <stdio.h>
#include <time.h>
#endif


void *
jml_reallocate(void *ptr,
    size_t old_size, size_t new_size)
{
    vm->allocated += new_size - old_size;

    if (new_size > old_size) {
#ifdef JML_STRESS_GC
        jml_gc_collect(vm);
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
    if (result == NULL) exit(EXIT_FAILURE);

    return result;
}


void *
jml_realloc(void *ptr,
    size_t new_size)
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
    if (result == NULL) exit(EXIT_FAILURE);

    return result;
}


static void
jml_gc_mark_roots(void)
{
    for (jml_value_t *slot = vm->stack;
        slot < vm->stack_top; slot++) {

        jml_gc_mark_value(*slot);
    }

    for (int i = 0; i < vm->frame_count; i++) {
        jml_gc_mark_obj((jml_obj_t*)vm->frames[i].closure);
    }

    for (jml_obj_upvalue_t *upvalue = vm->open_upvalues;
        upvalue != NULL;
        upvalue = upvalue->next) {
        jml_gc_mark_obj((jml_obj_t*)upvalue);
    }

    jml_hashmap_mark(&vm->globals);
    jml_compiler_mark_roots();
    jml_gc_mark_obj((jml_obj_t*)vm->init_string);
}


void
jml_gc_mark_obj(jml_obj_t *object)
{
    if (object == NULL) return;
    if (object->marked) return;

#ifdef JML_TRACE_GC
    printf("[GC]  |%p marked ", (void*)object);
    jml_obj_print(OBJ_VAL(object));
    printf("|\n");
#endif

    object->marked = true;
    if (vm->gray_capacity < vm->gray_count + 1) {
        vm->gray_capacity = GROW_CAPACITY(vm->gray_capacity);
        vm->gray_stack = (jml_obj_t**)realloc(vm->gray_stack,
            sizeof(jml_obj_t*) * vm->gray_capacity);

        if (vm->gray_stack == NULL) exit(EXIT_FAILURE);
    }
    vm->gray_stack[vm->gray_count++] = object;
}


void
jml_gc_mark_value(jml_value_t value)
{
    if (!IS_OBJ(value)) return;
    jml_gc_mark_obj(AS_OBJ(value));
}


static void
jml_gc_mark_array(jml_value_array_t *array)
{
    for (int i = 0; i < array->count; i++) {
        jml_gc_mark_value(array->values[i]);
    }
}


static void
jml_free_object(jml_obj_t *object)
{
#ifdef JML_TRACE_MEM
    printf(
        "[MEM] |%p freed (type %s)|\n",
        (void*)object,
        jml_obj_type_stringify(object->type)
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
            jml_obj_array_t *array = (jml_obj_array_t*)object;
            FREE_ARRAY(jml_value_array_t*, array->values->values, array->values->count);
            FREE(jml_obj_array_t, object);
            break;
        }

        case OBJ_MAP: {
            jml_obj_map_t *map = (jml_obj_map_t*)object;
            jml_hashmap_free(map->hashmap);
            FREE(jml_obj_map_t, object);
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
            FREE(jml_obj_upvalue_t, object);
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

        case OBJ_MODULE: {
            FREE(jml_obj_module_t, object);
            break;
        }
    }
}


void
jml_gc_free_objs(void)
{
    jml_obj_t *object       = vm->objects;

    while (object != NULL) {
        jml_obj_t *next     = object->next;
        jml_free_object(object);
        object              = next;
    }

    free(vm->gray_stack);
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
        } else {
            jml_obj_t *unreached = object;
            object               = object->next;

            if (previous != NULL) {
                previous->next   = object;
            } else {
                vm->objects      = object;
            }

            jml_free_object(unreached);
        }
    }
}


static void
jml_gc_blacken_obj(jml_obj_t *object)
{
#ifdef JML_TRACE_GC
    printf("[GC]  |%p blackened ", (void*)object);
    jml_value_print(OBJ_VAL(object));
    printf("|\n");
#endif

    switch (object->type) {
        case OBJ_INSTANCE: {
            jml_obj_instance_t *instance = (jml_obj_instance_t*)object;
            jml_gc_mark_obj((jml_obj_t*)instance->klass);
            jml_hashmap_mark(&instance->fields);
            break;
        }

        case OBJ_CLASS: {
            jml_obj_class_t *klass = (jml_obj_class_t*)object;
            jml_gc_mark_obj((jml_obj_t*)klass->name);
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
            for (int i = 0; i < closure->upvalue_count; i++) {
                jml_gc_mark_obj((jml_obj_t*)closure->upvalues[i]);
            }
            break;
        }

        case OBJ_FUNCTION: {
            jml_obj_function_t *function = (jml_obj_function_t*)object;
            jml_gc_mark_obj((jml_obj_t*)function->name);
            jml_gc_mark_array(&function->bytecode.constants);
            break;
        }

        case OBJ_UPVALUE: {
            jml_gc_mark_value(((jml_obj_upvalue_t*)object)->closed);
            break;
        }

        case OBJ_CFUNCTION:
        case OBJ_EXCEPTION:
        case OBJ_MODULE:
        case OBJ_STRING:
            break;

        case OBJ_ARRAY:
        case OBJ_MAP:
            /*TODO*/
            break;
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
#ifdef JML_TRACE_GC
    size_t before = vm->allocated;
    time_t start  = clock();
    printf(
        "[GC]  |gc started {current: %zd bytes}|\n", before
    );
#endif

    jml_gc_mark_roots();
    jml_gc_trace_refs();
    jml_hashmap_remove_white(&vm->strings);
    jml_gc_sweep();

    vm->next_gc = vm->allocated * GC_HEAP_GROW_FACTOR;

#ifdef JML_TRACE_GC
    time_t elapsed = clock() - start;
    size_t after = vm->allocated;
    printf(
        "[GC]  |gc ended {current: %zd bytes, collected: %zd bytes, next: %zd bytes, elapsed:%.3lds}|\n",
        after, before - after, vm->next_gc, (long)elapsed
    );
#endif
}
