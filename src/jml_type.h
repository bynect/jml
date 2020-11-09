#ifndef _JML_TYPE_H_
#define _JML_TYPE_H_

#include <jml.h>

#include <jml_value.h>
#include <jml_bytecode.h>


#define OBJ_TYPE(value)             (AS_OBJ(value)->type)


#define IS_STRING(value)            jml_is_obj(value, OBJ_STRING)
#define IS_ARRAY(value)             jml_is_obj(value, OBJ_ARRAY)
#define IS_MAP(value)               jml_is_obj(value, OBJ_MAP)
#define IS_CLASS(value)             jml_is_obj(value, OBJ_CLASS)
#define IS_INSTANCE(value)          jml_is_obj(value, OBJ_INSTANCE)
#define IS_METHOD(value)            jml_is_obj(value, OBJ_METHOD)
#define IS_FUNCTION(value)          jml_is_obj(value, OBJ_FUNCTION)
#define IS_CLOSURE(value)           jml_is_obj(value, OBJ_CLOSURE)
#define IS_CFUNCTION(value)         jml_is_obj(value, OBJ_CFUNCTION)
#define IS_EXCEPTION(value)         jml_is_obj(value, OBJ_EXCEPTION)


#define AS_STRING(value)            ((jml_obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value)           (((jml_obj_string_t*)AS_OBJ(value))->chars)
#define AS_ARRAY(value)             ((jml_obj_array_t*)AS_OBJ(value))
#define AS_MAP(value)               ((jml_obj_map_t*)AS_OBJ(value))
#define AS_CLASS(value)             ((jml_obj_class_t*)AS_OBJ(value))
#define AS_INSTANCE(value)          ((jml_obj_instance_t*)AS_OBJ(value))
#define AS_METHOD(value)            ((jml_obj_method_t*)AS_OBJ(value))
#define AS_FUNCTION(value)          ((jml_obj_function_t*)AS_OBJ(value))
#define AS_CLOSURE(value)           ((jml_obj_closure_t*)AS_OBJ(value))
#define AS_CFUNCTION(value)         (((jml_obj_cfunction_t*)AS_OBJ(value)))
#define AS_EXCEPTION(value)         ((jml_obj_exception_t*)AS_OBJ(value))


typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_METHOD,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_CFUNCTION,
    OBJ_EXCEPTION
} jml_obj_type;


struct jml_obj_s {
    jml_obj_type                    type;
    bool                            marked;
    struct jml_obj_s               *next;
};


struct jml_string_s {
    jml_obj_t                       obj;
    char                           *chars;
    size_t                          length;
    uint32_t                        hash;
};


typedef struct {
    jml_obj_t                       obj;
    jml_obj_string_t               *name;
    jml_obj_string_t               *message;
} jml_obj_exception_t;


typedef struct {
    jml_obj_t                       obj;
    jml_value_array_t              *values;
} jml_obj_array_t;


typedef struct {
    jml_obj_t                       obj;
    jml_hashmap_t                  *hashmap;
} jml_obj_map_t;


typedef struct {
    jml_obj_t                       obj;
    jml_obj_string_t               *name;
    jml_hashmap_t                   methods;
} jml_obj_class_t;


typedef struct {
    jml_obj_t                       obj;
    jml_obj_class_t                *klass;
    jml_hashmap_t                   fields;
} jml_obj_instance_t;


typedef struct {
    jml_obj_t                       obj;
    int                             arity;
    int                             upvalue_count;
    jml_bytecode_t                  bytecode;
    jml_obj_string_t               *name;
} jml_obj_function_t;


typedef struct jml_obj_upvalue_t {
    jml_obj_t                       obj;
    jml_value_t                    *location;
    jml_value_t                     closed;
    struct jml_obj_upvalue_t       *next;
} jml_obj_upvalue_t;


typedef struct {
    jml_obj_t                       obj;
    jml_obj_function_t             *function;
    jml_obj_upvalue_t             **upvalues;
    uint8_t                         upvalue_count;
} jml_obj_closure_t;


typedef struct {
    jml_obj_t                       obj;
    jml_value_t                     receiver;
    jml_obj_closure_t              *method;
} jml_obj_method_t;


typedef jml_value_t (*jml_cfunction)(int arg_count, jml_value_t *args);


typedef struct {
    jml_obj_t                       obj;
    jml_obj_string_t               *name;
    jml_cfunction                   function;
} jml_obj_cfunction_t;


jml_obj_string_t *jml_obj_string_take(char *chars,
    size_t length);

jml_obj_string_t *jml_obj_string_copy(const char *chars,
    size_t length);

jml_obj_array_t *jml_obj_array_new(void);

jml_obj_map_t *jml_obj_map_new(void);

jml_obj_class_t *jml_obj_class_new(jml_obj_string_t *name);

jml_obj_instance_t *jml_obj_instance_new(jml_obj_class_t *klass);

jml_obj_method_t *jml_obj_method_new(jml_value_t receiver,
    jml_obj_closure_t *method);

jml_obj_function_t *jml_obj_function_new(void);

jml_obj_closure_t *jml_obj_closure_new(jml_obj_function_t *function);

jml_obj_upvalue_t *jml_obj_upvalue_new(jml_value_t *slot);

jml_obj_cfunction_t *jml_obj_cfunction_new(
    const char *name, jml_cfunction function);

jml_obj_exception_t *jml_obj_exception_new(const char *name,
    const char *message);

void jml_obj_print(jml_value_t value);

char *jml_obj_stringify(jml_value_t value);


static inline bool
jml_is_obj(jml_value_t value, jml_obj_type type)
{
    return IS_OBJ(value)
        && AS_OBJ(value)->type == type;
}


#endif /* _JML_TYPE_H_ */
