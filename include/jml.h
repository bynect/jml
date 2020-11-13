#ifndef _JML_H_
#define _JML_H_

#ifdef __cplusplus
extern "C" {
#endif


#define JML_VERSION_MAJOR           0
#define JML_VERSION_MINOR           1
#define JML_VERSION_MICRO           0
#define JML_VERSION_STRING          "0.1.0"


#if defined (_WIN64)

#define JML_PLATFORM_STRING         "win64"
#define JML_PLATFORM                7

#elif defined (_WIN32)

#define JML_PLATFORM_STRING         "win32"
#define JML_PLATFORM                6

#elif defined (__APPLE__) && (defined (TARGET_OS_MAC) || defined (__MACH__))

#define JML_PLATFORM_STRING         "macos"
#define JML_PLATFORM                5

#elif defined (__linux__) || (defined (__linux))

#define JML_PLATFORM_STRING         "linux"
#define JML_PLATFORM                4

#elif defined (BSD)

#define JML_PLATFORM_STRING         "bsd"
#define JML_PLATFORM                3

#elif defined (_POSIX_VERSION)

#define JML_PLATFORM_STRING         "posix"
#define JML_PLATFORM                2

#elif defined (__unix__)

#define JML_PLATFORM_STRING         "unix"
#define JML_PLATFORM                1

#else

#define JML_PLATFORM_STRING         "other"
#define JML_PLATFORM                0

#endif


typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} jml_interpret_result;


typedef struct jml_vm_s jml_vm_t;


jml_vm_t *jml_vm_new(void);

void jml_vm_free(jml_vm_t *vm_ptr);

jml_interpret_result jml_vm_interpret(const char *source);


#ifdef __cplusplus
}
#endif

#endif /* _JML_H_ */