#ifndef _JML_GC_H_
#define _JML_GC_H_

#include <jml_common.h>
#include <jml_type.h>
#include <jml_compiler.h>


#define GC_HEAP_GROW_FACTOR 1.5

#define ALLOCATE(type, count)                           \
    (type*)jml_reallocate(NULL, 0UL, sizeof(type) * (count))

#define FREE(type, ptr)                                 \
    jml_reallocate(ptr, sizeof(type), 0UL)

#define GROW_CAPACITY(capacity)                         \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, ptr, old_count, new_count)     \
    (type*)jml_reallocate(ptr,                          \
        sizeof(type) * (old_count),                     \
        sizeof(type) * (new_count))

#define FREE_ARRAY(type, ptr, old_count)                \
    jml_reallocate(ptr, sizeof(type) * (old_count), 0UL)

#define REALLOC(type, ptr, size, dest_size)             \
    if (size <= dest_size) {                            \
        do {                                            \
            size *= GC_HEAP_GROW_FACTOR;                \
            ptr = (type*)jml_realloc(ptr, size);        \
        } while (size <= dest_size);                    \
    }


void *jml_reallocate(void *ptr,
    size_t old_size, size_t new_size);

void *jml_realloc(void *ptr, size_t new_size);

void jml_gc_free_objs(void);

void jml_gc_collect(void);

void jml_gc_mark_value(jml_value_t value);

void jml_gc_mark_obj(jml_obj_t *object);


#endif /* _JML_GC_H_ */
