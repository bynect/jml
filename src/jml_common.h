#ifndef _JML_COMMON_H_
#define _JML_COMMON_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOCAL_MAX                   (UINT8_MAX + 1)
#define FRAMES_MAX                  64
#define STACK_MAX                   (FRAMES_MAX * LOCAL_MAX)

#undef JML_NAN_TAGGING
#define JML_DISASSEMBLE
#undef JML_STRESS_GC
#undef JML_TRACE_GC
#define JML_PRINT_TOKEN


#endif /* _JML_COMMON_H_ */
