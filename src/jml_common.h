#ifndef _JML_COMMON_H_
#define _JML_COMMON_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define LOCAL_MAX                   (UINT8_MAX + 1)
#define FRAMES_MAX                  64
#define STACK_MAX                   (FRAMES_MAX * LOCAL_MAX)
#define MAP_LOAD_MAX                0.75


#define JML_NAN_TAGGING
#define JML_DISASSEMBLE
#undef JML_STRESS_GC
#undef JML_TRACE_GC
#undef JML_PRINT_TOKEN
#define JML_DEBUG


#ifdef JML_DEBUG

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(cond, msg)                               \
    do {                                                \
        if (!cond) {                                    \
            fprintf(                                    \
                stderr, "[%s:%d] Assert failed %s:%s\n",\
                __FILE__, __LINE__, __func__, msg       \
            );                                          \
        }                                               \
        abort();                                        \
    } while (false)

#define UNREACHABLE()                                   \
    do {                                                \
        fprintf(                                        \
            stderr,"[%s:%d] Hit unreachable code %s\n", \
            __FILE__, __LINE__, __func__                \
        );                                              \
        abort();                                        \
    } while (false)

#else

#define ASSERT(cond, msg) do { } while (false)
#define UNREACHABLE()

#endif


#endif /* _JML_COMMON_H_ */
