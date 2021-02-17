#ifndef JML_COMMON_H_
#define JML_COMMON_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define LOCAL_MAX                   (UINT8_MAX + 1)
#define FRAMES_MAX                  128
#define STACK_MAX                   (FRAMES_MAX * LOCAL_MAX)
#define MAP_LOAD_MAX                0.75
#define EXEMPT_MAX                  32

#define JML_BACKTRACE
#define JML_RECURSIVE_SEARCH
#undef  JML_LAZY_IMPORT
#define JML_EVAL


#ifdef JML_NDEBUG

#define JML_NAN_TAGGING
#undef  JML_DISASSEMBLE
#undef  JML_TRACE_STACK
#undef  JML_TRACE_SLOT
#undef  JML_STEP_STACK
#undef  JML_STRESS_GC
#undef  JML_TRACE_GC
#undef  JML_ROUND_GC
#undef  JML_TRACE_MEM
#undef  JML_PRINT_TOKEN
#undef  JML_ASSERTION

#else

#define JML_NAN_TAGGING
#define JML_DISASSEMBLE
#define JML_TRACE_STACK
#define JML_TRACE_SLOT
#undef  JML_STEP_STACK
#undef  JML_STRESS_GC
#undef  JML_TRACE_GC
#undef  JML_ROUND_GC
#undef  JML_TRACE_MEM
#undef  JML_PRINT_TOKEN
#undef  JML_ASSERTION

#endif


#ifdef JML_ASSERTION

#include <stdio.h>
#include <stdlib.h>

#define JML_ASSERT(condition, format, ...)              \
    do {                                                \
        if (!condition) {                               \
            fprintf(                                    \
                stderr,                                 \
                "[%s:%d] Assertion failed in %s: ",     \
                __FILE__, __LINE__, __func__            \
            );                                          \
            fprintf(                                    \
                stderr,                                 \
                format, __VA_ARGS__                     \
            );                                          \
            abort();                                    \
        }                                               \
    } while (false)

#define JML_UNREACHABLE()                               \
    do {                                                \
        fprintf(                                        \
            stderr,                                     \
            "[%s:%d] Unreachable code hit in %s\n",     \
            __FILE__, __LINE__, __func__                \
        );                                              \
        abort();                                        \
    } while (false)

#else

#define JML_ASSERT(condition, format, ...)
#define JML_UNREACHABLE()

#endif


#endif /* JML_COMMON_H_ */
