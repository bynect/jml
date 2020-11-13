#ifndef JML_COMMON_H_
#define JML_COMMON_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define LOCAL_MAX                   (UINT8_MAX + 1)
#define FRAMES_MAX                  64
#define STACK_MAX                   (FRAMES_MAX * LOCAL_MAX)
#define MAP_LOAD_MAX                0.75


#ifdef __GNUC__

#define JML_COMPUTED_GOTO
#define JML_UNUSED(arg)             __attribute__((unused)) arg

#else

#undef  JML_COMPUTED_GOTO
#define JML_UNUSED(arg)             arg

#endif


#ifdef JML_NDEBUG

#define JML_NAN_TAGGING
#undef  JML_DISASSEMBLE
#undef  JML_STRESS_GC
#undef  JML_TRACE_GC
#undef  JML_TRACE_MEM
#undef  JML_PRINT_TOKEN
#undef  JML_DEBUG

#else

#define JML_NAN_TAGGING
#define JML_DISASSEMBLE
#undef  JML_STRESS_GC
#define JML_TRACE_GC
#undef  JML_TRACE_MEM
#undef  JML_PRINT_TOKEN
#undef  JML_DEBUG

#endif


#ifdef JML_DEBUG

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(condition, format, ...)                  \
    do {                                                \
        if (!condition) {                               \
            fprintf(                                    \
                stderr,                                 \
                "[%s:%d] Assert failed in %s: ",        \
                __FILE__, __LINE__, __func__            \
            );                                          \
            fprintf(                                    \
                stderr,                                 \
                format, __VA_ARGS__                     \
            );                                          \
            exit(EXIT_FAILURE);                         \
        }                                               \
    } while (false)

#define UNREACHABLE()                                   \
    do {                                                \
        fprintf(                                        \
            stderr,"[%s:%d] Hit unreachable code %s\n", \
            __FILE__, __LINE__, __func__                \
        );                                              \
        exit(EXIT_FAILURE);                             \
    } while (false)

#else

#define ASSERT(condition, format, ...) do { } while (false)
#define UNREACHABLE()

#endif


#endif /* JML_COMMON_H_ */
