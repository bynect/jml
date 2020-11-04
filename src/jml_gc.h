#ifndef _JML_GC_H_
#define _JML_GC_H_

#include <jml_common.h>
#include <jml_type.h>
#include <jml_value.h>
#include <jml_vm.h>


#define ALLOCATE(type, count)                           \
    (type*)jml_reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, ptr)                                 \
    jml_reallocate(ptr, sizeof(type), 0)

#define GROW_CAPACITY(capacity)                         \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, ptr, old_count, new_count)     \
    (type*)jml_reallocate(ptr,                          \
        sizeof(type) * (old_count),                     \
        sizeof(type) * (new_count))

#define FREE_ARRAY(type, ptr, old_count)                \
    jml_reallocate(ptr, sizeof(type) * (old_count), 0)


void *jml_reallocate(void *ptr,
    size_t old_size, size_t new_size);


typedef struct {
    jml_vm_t                       *vm;

    size_t                          allocated;
    size_t                          next_gc;
    jml_obj_t                      *objects;
    int                             gray_count;
    int                             gray_capacity;
    jml_obj_t                     **gray_stack;
} jml_gc_t;


void jml_gc_init(jml_gc_t *gc, jml_vm_t *vm);
void jml_gc_free(jml_gc_t *gc);

void jml_gc_free_objs(void);
void jml_gc_collect(jml_gc_t *gc);

void jml_gc_mark_value(jml_value_t value);
void jml_gc_mark_obj(jml_obj_t *object);


extern jml_gc_t *gc;


#endif /* _JML_GC_H_ */
