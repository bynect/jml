#ifndef JML_TYPE_H_
#define JML_TYPE_H_

#include <jml.h>

#include <jml_value.h>
#include <jml_bytecode.h>


#define OBJ_TYPE(value)             (AS_OBJ(value)->type)


#define IS_STRING(value)            jml_obj_has_type(value, OBJ_STRING)
#define IS_ARRAY(value)             jml_obj_has_type(value, OBJ_ARRAY)
#define IS_MAP(value)               jml_obj_has_type(value, OBJ_MAP)
#define IS_MODULE(value)            jml_obj_has_type(value, OBJ_MODULE)
#define IS_CLASS(value)             jml_obj_has_type(value, OBJ_CLASS)
#define IS_INSTANCE(value)          jml_obj_has_type(value, OBJ_INSTANCE)
#define IS_METHOD(value)            jml_obj_has_type(value, OBJ_METHOD)
#define IS_FUNCTION(value)          jml_obj_has_type(value, OBJ_FUNCTION)
#define IS_CLOSURE(value)           jml_obj_has_type(value, OBJ_CLOSURE)
#define IS_COROUTINE(value)         jml_obj_has_type(value, OBJ_COROUTINE)
#define IS_CFUNCTION(value)         jml_obj_has_type(value, OBJ_CFUNCTION)
#define IS_EXCEPTION(value)         jml_obj_has_type(value, OBJ_EXCEPTION)


#define AS_STRING(value)            ((jml_obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value)           (((jml_obj_string_t*)AS_OBJ(value))->chars)
#define AS_ARRAY(value)             ((jml_obj_array_t*)AS_OBJ(value))
#define AS_MAP(value)               ((jml_obj_map_t*)AS_OBJ(value))
#define AS_MODULE(value)            ((jml_obj_module_t*)AS_OBJ(value))
#define AS_CLASS(value)             ((jml_obj_class_t*)AS_OBJ(value))
#define AS_INSTANCE(value)          ((jml_obj_instance_t*)AS_OBJ(value))
#define AS_METHOD(value)            ((jml_obj_method_t*)AS_OBJ(value))
#define AS_FUNCTION(value)          ((jml_obj_function_t*)AS_OBJ(value))
#define AS_CLOSURE(value)           ((jml_obj_closure_t*)AS_OBJ(value))
#define AS_COROUTINE(value)         ((jml_obj_coroutine_t*)AS_OBJ(value))
#define AS_CFUNCTION(value)         (((jml_obj_cfunction_t*)AS_OBJ(value)))
#define AS_EXCEPTION(value)         ((jml_obj_exception_t*)AS_OBJ(value))


typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_MODULE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_METHOD,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_COROUTINE,
    OBJ_CFUNCTION,
    OBJ_EXCEPTION
} jml_obj_type;


struct jml_obj {
    jml_obj_type                    type;
    bool                            marked;
    struct jml_obj                 *next;
};


struct jml_obj_string {
    jml_obj_t                       obj;
    char                           *chars;
    size_t                          length;
    uint32_t                        hash;
};


struct jml_obj_array {
    jml_obj_t                       obj;
    jml_value_array_t               values;
};


struct jml_obj_map {
    jml_obj_t                       obj;
    jml_hashmap_t                   hashmap;
};


#ifdef JML_PLATFORM_WIN

#include <windows.h>

typedef HINSTANCE jml_module_handle_t;

#else

typedef void *jml_module_handle_t;

#endif


struct jml_obj_module {
    jml_obj_t                       obj;
    jml_obj_string_t               *name;
    jml_module_handle_t             handle;
    jml_hashmap_t                   globals;
};


struct jml_obj_class {
    jml_obj_t                       obj;
    jml_obj_string_t               *name;
    jml_hashmap_t                   statics;
    bool                            inheritable;
    struct jml_obj_class           *super;
    jml_obj_module_t               *module;
};


struct jml_obj_instance {
    jml_obj_t                       obj;
    jml_obj_class_t                *klass;
    jml_hashmap_t                   fields;
    void                           *extra;
};


struct jml_obj_method {
    jml_obj_t                       obj;
    jml_value_t                     receiver;
    jml_obj_closure_t              *method;
};


struct jml_obj_function {
    jml_obj_t                       obj;
    int                             arity;
    bool                            variadic;
    int                             upvalue_count;
    jml_bytecode_t                  bytecode;
    jml_obj_string_t               *name;
    jml_obj_string_t               *klass_name;
    jml_obj_module_t               *module;
};


struct jml_obj_closure {
    jml_obj_t                       obj;
    jml_obj_function_t             *function;
    jml_obj_upvalue_t             **upvalues;
    uint8_t                         upvalue_count;
};


struct jml_obj_upvalue {
    jml_obj_t                       obj;
    jml_value_t                    *location;
    jml_value_t                     closed;
    struct jml_obj_upvalue         *next;
};


struct jml_obj_coroutine {
    jml_obj_t                       obj;
    jml_call_frame_t                frames[FRAMES_MAX];
    uint8_t                         frame_count;
    jml_obj_upvalue_t              *open_upvalues;

    jml_value_t                     stack[STACK_MAX];
    jml_value_t                    *stack_top;
    struct jml_obj_coroutine       *caller;
};


typedef jml_value_t (*jml_cfunction)(int arg_count, jml_value_t *args);

typedef void (*jml_mfunction)(jml_obj_module_t *module);


struct jml_obj_cfunction {
    jml_obj_t                       obj;
    jml_obj_string_t               *name;
    jml_cfunction                   function;
    jml_obj_string_t               *klass_name;
    jml_obj_module_t               *module;
};


struct jml_obj_exception {
    jml_obj_t                       obj;
    jml_obj_string_t               *name;
    jml_obj_string_t               *message;
    jml_obj_module_t               *module;
};


jml_obj_string_t *jml_obj_string_take(char *chars,
    size_t length);

jml_obj_string_t *jml_obj_string_copy(const char *chars,
    size_t length);

jml_obj_array_t *jml_obj_array_new(void);

void jml_obj_array_append(jml_obj_array_t *array, jml_value_t value);

void jml_obj_array_add(jml_obj_array_t *source, jml_obj_array_t *dest);

jml_obj_map_t *jml_obj_map_new(void);

jml_obj_module_t *jml_obj_module_new(jml_obj_string_t *name, void *handle);

jml_obj_class_t *jml_obj_class_new(jml_obj_string_t *name);

jml_obj_instance_t *jml_obj_instance_new(jml_obj_class_t *klass);

jml_obj_method_t *jml_obj_method_new(jml_value_t receiver,
    jml_obj_closure_t *method);

jml_obj_function_t *jml_obj_function_new(void);

jml_obj_closure_t *jml_obj_closure_new(jml_obj_function_t *function);

jml_obj_upvalue_t *jml_obj_upvalue_new(jml_value_t *slot);

jml_obj_coroutine_t *jml_obj_coroutine_new(jml_obj_closure_t *closure);

jml_obj_cfunction_t *jml_obj_cfunction_new(jml_obj_string_t *name,
    jml_cfunction function, jml_obj_module_t *module);

jml_obj_exception_t *jml_obj_exception_new(const char *name,
    const char *message);

jml_obj_exception_t *jml_obj_exception_format(const char *name,
    char *message_format, ...);


static inline bool
jml_obj_has_type(jml_value_t value, jml_obj_type type)
{
    return IS_OBJ(value)
        && AS_OBJ(value)->type == type;
}


#endif /* JML_TYPE_H_ */
