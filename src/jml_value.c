#include <string.h>
#include <stdio.h>

#include <jml_common.h>
#include <jml_value.h>
#include <jml_type.h>
#include <jml_gc.h>


void
jml_value_print(jml_value_t value)
{
#ifdef JML_NAN_TAGGING
    if (IS_BOOL(value)) {
        printf(AS_BOOL(value) ? "true" : "false");
    } else if (IS_NONE(value)) {
        printf("none");
    } else if (IS_NUMBER(value)) {
        printf("%g", AS_NUMBER(value));
    } else if (IS_OBJ(value)) {
        jml_obj_print(value);
    }
#else
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;

        case VAL_NONE:
            printf("none");
            break;

        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;

        case VAL_OBJ:
            jml_obj_print(value);
            break;
    }
#endif
}


bool
jml_value_equal(jml_value_t a, jml_value_t b)
{

}





void
jml_value_array_init(jml_value_array_t *array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}


void
jml_value_array_write(jml_value_array_t *array,
    jml_value_t value)
{
    if (array->capacity < array->count + 1) {
        int old_capacity = array->count;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(jml_value_t,
            array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}


void
jml_value_array_free(jml_value_array_t *array)
{
    FREE_ARRAY(jml_value_t, array->values, array->capacity);
    jml_value_array_init(array);
}







void
jml_hashmap_init(jml_hashmap_t *map)
{

}


void
jml_hashmap_free(jml_hashmap_t *map)
{

}


void
jml_hashmap_remove_white(jml_value_array_t *array)
{
    FREE_ARRAY(jml_value_t, array->values,
        array->capacity);
    jml_value_array_init(array);
}


void
jml_hashmap_mark(jml_hashmap_t *map)
{

}


bool
jml_hashmap_get(jml_hashmap_t *map,
    jml_obj_string_t *key, jml_value_t *value)
{

}


bool
jml_hashmap_set(jml_hashmap_t *map,
    jml_obj_string_t *key, jml_value_t value)
{

}


bool
jml_hashmap_del(jml_hashmap_t *map,
    jml_obj_string_t *key)
{

}


void
jml_hashmap_add(jml_hashmap_t *source,
    jml_hashmap_t *dest)
{

}


jml_obj_string_t *
jml_hashmap_find(jml_hashmap_t *map, const char *chars,
    size_t length, uint32_t hash)
{

}










