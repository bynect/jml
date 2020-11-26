#ifndef JML_H_
#define JML_H_

/*INFO*/
#define JML_VERSION_MAJOR           0
#define JML_VERSION_MINOR           1
#define JML_VERSION_MICRO           0
#define JML_VERSION_STRING          "0.1.0"


/*API*/
typedef struct jml_obj_s            jml_obj_t;
typedef struct jml_obj_string_s     jml_obj_string_t;
typedef struct jml_obj_array_s      jml_obj_array_t;
typedef struct jml_obj_map_s        jml_obj_map_t;
typedef struct jml_obj_module_s     jml_obj_module_t;
typedef struct jml_obj_class_s      jml_obj_class_t;
typedef struct jml_obj_instance_s   jml_obj_instance_t;
typedef struct jml_obj_function_s   jml_obj_function_t;
typedef struct jml_obj_upvalue_s    jml_obj_upvalue_t;
typedef struct jml_obj_closure_s    jml_obj_closure_t;
typedef struct jml_obj_method_s     jml_obj_method_t;
typedef struct jml_obj_cfunction_s  jml_obj_cfunction_t;
typedef struct jml_obj_exception_s  jml_obj_exception_t;

#include <jml_value.h>
#include <jml_type.h>


typedef jml_value_t (*jml_cfunction)(int arg_count, jml_value_t *args);

typedef void (*jml_mfunction)(jml_obj_module_t *module);


typedef struct {
    const char                     *name;
    jml_cfunction                   function;
} jml_module_function;


#ifdef __cplusplus

#define MODULE_TABLE_HEAD           extern "C" jml_module_function module_table[]
#define MODULE_FUNC_HEAD            extern "C" void

#else

#define MODULE_TABLE_HEAD           jml_module_function module_table[]
#define MODULE_FUNC_HEAD            void

#endif


jml_obj_exception_t *jml_core_exception_args(
    int arg_count, int expected_arg);

jml_obj_exception_t *jml_core_exception_implemented(
    jml_value_t value);

jml_obj_exception_t *jml_core_exception_types(
    bool mult, int arg_count, ...);


typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} jml_interpret_result;


typedef struct jml_vm_s jml_vm_t;


jml_vm_t *jml_vm_new(void);

void jml_vm_free(jml_vm_t *vm_ptr);

jml_interpret_result jml_vm_interpret(const char *source);


jml_value_t jml_string_intern(const char *string);

void jml_gc_exempt_push(jml_value_t value);

jml_value_t jml_gc_exempt_pop(void);

jml_value_t jml_gc_exempt_peek(int distance);


/*UTILITY*/
void *jml_realloc(void *ptr, size_t new_size);


char *jml_strsep(char **str, const char *sep);

char *jml_strtok(char *input, const char *delimiter);

char *jml_strdup(const char *str);

char *jml_strcat(char *dest, char *src);


/*MACRO*/
#ifdef __GNUC__

#define JML_COMPUTED_GOTO
#define JML_UNUSED(arg)             __attribute__((unused)) arg

#else

#define JML_UNUSED(arg)             arg

#endif


#if defined (_WIN64) || (defined (WIN64))

#define JML_PLATFORM_WIN

#define JML_PLATFORM_STRING         "win64"
#define JML_PLATFORM                7

#elif defined (_WIN32) || (defined (WIN32))

#define JML_PLATFORM_WIN

#define JML_PLATFORM_STRING         "win32"
#define JML_PLATFORM                6

#elif defined (__APPLE__) && (defined (TARGET_OS_MAC) || defined (__MACH__))

#define JML_PLATFORM_MAC
#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "macos"
#define JML_PLATFORM                5

#elif defined (__linux__) || (defined (__linux))

#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "linux"
#define JML_PLATFORM                4

#elif defined (BSD)

#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "bsd"
#define JML_PLATFORM                3

#elif defined (_POSIX_VERSION)

#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "posix"
#define JML_PLATFORM                2

#elif defined (__unix__)

#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "unix"
#define JML_PLATFORM                1

#else

#define JML_PLATFORM_UNK

#define JML_PLATFORM_STRING         "other"
#define JML_PLATFORM                0

#endif


#endif /* JML_H_ */
