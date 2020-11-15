#ifndef JML_VALUE_H_
#define JML_VALUE_H_

#include <string.h>

#include <jml_common.h>


typedef struct jml_obj_s        jml_obj_t;
typedef struct jml_string_s     jml_obj_string_t;


#ifdef JML_NAN_TAGGING

#define SIGN_BIT                    ((uint64_t)0x8000000000000000)
#define QNAN                        ((uint64_t)0x7ffc000000000000)

#define TAG_NONE                    1
#define TAG_FALSE                   2
#define TAG_TRUE                    3


typedef uint64_t                    jml_value_t;


#define FALSE_VAL                   ((jml_value_t)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL                    ((jml_value_t)(uint64_t)(QNAN | TAG_TRUE))
#define BOOL_VAL(b)                 ((b) ? TRUE_VAL : FALSE_VAL)
#define NONE_VAL                    ((jml_value_t)(uint64_t)(QNAN | TAG_NONE))
#define NUM_VAL(num)                jml_num_to_val(num)
#define OBJ_VAL(obj)                                    \
    (jml_value_t)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline jml_value_t
jml_num_to_val(double num)
{
    jml_value_t value;
    memcpy(&value, &num, sizeof(double));
    return value;
}


#define AS_BOOL(value)              ((value) == TRUE_VAL)
#define AS_NUM(value)            jml_value_to_num(value)
#define AS_OBJ(value)                                   \
    ((jml_obj_t*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

static inline double
jml_value_to_num(jml_value_t value)
{
  double num;
  memcpy(&num, &value, sizeof(jml_value_t));
  return num;
}


#define IS_BOOL(value)              (((value) | 1) == TRUE_VAL)
#define IS_NONE(value)              ((value) == NONE_VAL)
#define IS_NUM(value)               (((value) & QNAN) != QNAN)
#define IS_OBJ(value)                                   \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#else

typedef enum {
    VAL_BOOL,
    VAL_NONE,
    VAL_NUM,
    VAL_OBJ
} jml_value_type;


typedef struct {
    jml_value_type                  type;
    union {
        bool                        boolean;
        double                      number;
        jml_obj_t                  *obj;
    } of;
} jml_value_t;


#define BOOL_VAL(value)                                 \
    ((jml_value_t){VAL_BOOL, {.boolean = value}})
#define NONE_VAL                                        \
    ((jml_value_t){VAL_NONE, {.number = 0}})
#define NUM_VAL(value)                                  \
    ((jml_value_t){VAL_NUM, {.number = value}})
#define OBJ_VAL(object)                                 \
    ((jml_value_t){VAL_OBJ, {.obj = (jml_obj_t*)object}})

#define AS_BOOL(value)              ((value).of.boolean)
#define AS_NUM(value)               ((value).of.number)
#define AS_OBJ(value)               ((value).of.obj)

#define IS_BOOL(value)              ((value).type == VAL_BOOL)
#define IS_NONE(value)              ((value).type == VAL_NONE)
#define IS_NUM(value)               ((value).type == VAL_NUM)
#define IS_OBJ(value)               ((value).type == VAL_OBJ)

#endif


void jml_value_print(jml_value_t value);

char *jml_value_stringify(jml_value_t value);

char *jml_value_stringify_type(jml_value_t value);

bool jml_value_equal(jml_value_t a, jml_value_t b);


typedef struct {
    int                             capacity;
    int                             count;
    jml_value_t                    *values;
} jml_value_array_t;


void jml_value_array_init(jml_value_array_t *array);

void jml_value_array_write(jml_value_array_t *array,
    jml_value_t value);

void jml_value_array_free(jml_value_array_t *array);


typedef struct {
    jml_obj_string_t               *key;
    jml_value_t                     value;
} jml_hashmap_entry_t;


typedef struct {
    int                             count;
    int                             capacity;
    jml_hashmap_entry_t            *entries;
} jml_hashmap_t;


void jml_hashmap_init(jml_hashmap_t *map);

void jml_hashmap_free(jml_hashmap_t *map);

bool jml_hashmap_get(jml_hashmap_t *map, jml_obj_string_t *key,
    jml_value_t *value);

bool jml_hashmap_set(jml_hashmap_t *map, jml_obj_string_t *key,
    jml_value_t value);

bool jml_hashmap_del(jml_hashmap_t *map, jml_obj_string_t *key);

void jml_hashmap_add(jml_hashmap_t *source, jml_hashmap_t *dest);

jml_obj_string_t *jml_hashmap_find(jml_hashmap_t *map,
    const char *chars, size_t length, uint32_t hash);

void jml_hashmap_remove_white(jml_hashmap_t *map);

void jml_hashmap_mark(jml_hashmap_t *map);


static inline bool
jml_is_falsey(jml_value_t value)
{
    return (IS_NONE(value)
        || (IS_BOOL(value)
        && !AS_BOOL(value)));
}


#endif /* JML_VALUE_H_ */
