#ifndef JML_H_
#define JML_H_


/*INFO*/
#define JML_VERSION_MAJOR           0
#define JML_VERSION_MINOR           1
#define JML_VERSION_MICRO           0
#define JML_VERSION_STRING          "0.1.0"


#if defined _WIN64 || defined WIN64

#define JML_PLATFORM_WIN

#define JML_PLATFORM_STRING         "win64"
#define JML_PLATFORM                7

#elif defined _WIN32 || defined WIN32

#define JML_PLATFORM_WIN

#define JML_PLATFORM_STRING         "win32"
#define JML_PLATFORM                6

#elif defined __APPLE__ && defined TARGET_OS_MAC || defined __MACH__

#define JML_PLATFORM_MAC

#define JML_PLATFORM_STRING         "macos"
#define JML_PLATFORM                5

#elif defined __linux__ || defined __linux

#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "linux"
#define JML_PLATFORM                4

#elif defined BSD

#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "bsd"
#define JML_PLATFORM                3

#elif defined _POSIX_VERSION

#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "posix"
#define JML_PLATFORM                2

#elif defined __unix__

#define JML_PLATFORM_NIX

#define JML_PLATFORM_STRING         "unix"
#define JML_PLATFORM                1

#else

#warning Current platform not determinated.

#define JML_PLATFORM_UNK

#define JML_PLATFORM_STRING         "other"
#define JML_PLATFORM                0

#endif


/*MACRO*/
#ifdef __GNUC__

#define JML_COMPUTED_GOTO

#define JML_DEPRECATED              __attribute__((deprecated))
#define JML_UNUSED(arg)             __attribute__((unused)) arg
#define JML_FORMAT(a, b)            __attribute__((format(printf, a, b)))

#else

#define JML_DEPRECATED
#define JML_UNUSED(arg)             arg
#define JML_FORMAT(a, b)

#endif


#ifdef __cplusplus

#define JML_NOMANGLE                extern "C"

#else

#define JML_NOMANGLE

#endif


#ifdef JML_PLATFORM_WIN

#define JML_APIF                    __declspec(dllexport) JML_NOMANGLE
#define JML_APIM                    __declspec(dllexport)

#else

#define JML_APIF                    JML_NOMANGLE
#define JML_APIM

#endif


#define MODULE_TABLE_HEAD           JML_APIM jml_module_function
#define MODULE_FUNC_HEAD            JML_APIF void


/*API*/
typedef struct jml_obj              jml_obj_t;
typedef struct jml_obj_string       jml_obj_string_t;
typedef struct jml_obj_array        jml_obj_array_t;
typedef struct jml_obj_map          jml_obj_map_t;
typedef struct jml_obj_module       jml_obj_module_t;
typedef struct jml_obj_class        jml_obj_class_t;
typedef struct jml_obj_instance     jml_obj_instance_t;
typedef struct jml_obj_method       jml_obj_method_t;
typedef struct jml_obj_function     jml_obj_function_t;
typedef struct jml_obj_upvalue      jml_obj_upvalue_t;
typedef struct jml_obj_closure      jml_obj_closure_t;
typedef struct jml_obj_coroutine    jml_obj_coroutine_t;
typedef struct jml_obj_cfunction    jml_obj_cfunction_t;
typedef struct jml_obj_exception    jml_obj_exception_t;

#include <jml/jml_value.h>
#include <jml/jml_type.h>
#include <jml/jml_bytecode.h>


typedef jml_value_t (*jml_cfunction)(int arg_count, jml_value_t *args);

typedef void (*jml_mfunction)(jml_obj_module_t *module);


typedef struct {
    const char                     *name;
    jml_cfunction                   function;
} jml_module_function;


bool jml_module_add_value(jml_obj_module_t *module,
    const char *name, jml_value_t value);

bool jml_module_add_class(jml_obj_module_t *module,
    const char *name, jml_module_function *table, bool inheritable);


jml_obj_exception_t *jml_error_args(int arg_count, int expected_arg);

jml_obj_exception_t *jml_error_implemented(jml_value_t value);

jml_obj_exception_t *jml_error_types(bool mult, int arg_count, ...);

jml_obj_exception_t *jml_error_value(const char *value);


typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} jml_interpret_result;


typedef struct jml_vm jml_vm_t;


jml_vm_t *jml_vm_new(void);

void jml_vm_free(jml_vm_t *vm);

jml_interpret_result jml_vm_interpret(jml_vm_t *_vm, const char *source);

jml_interpret_result jml_vm_interpret_bytecode(jml_vm_t *_vm, jml_bytecode_t *bytecode);

jml_value_t jml_vm_eval(jml_vm_t *_vm, const char *source);


/*UTILITY*/
void *jml_realloc(void *ptr, size_t new_size);

void *jml_alloc(size_t size);

void jml_free(void *ptr);


void jml_gc_exempt_push(jml_value_t value);

jml_value_t jml_gc_exempt_pop(void);

jml_value_t jml_gc_exempt_peek(int distance);


jml_value_t jml_string_intern(const char *string);

size_t jml_string_len(const char *str, size_t size);


#include <jml/jml_util.h>

#include <jml/jml_repr.h>


#endif /* JML_H_ */
