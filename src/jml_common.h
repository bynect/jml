#ifndef _JML_COMMON_H_
#define _JML_COMMON_H_

#define JML_VERSION_MAJOR           0
#define JML_VERSION_MINOR           1
#define JML_VERSION_MICRO           0

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOCAL_MAX                   (UINT8_MAX + 1)

#undef JML_NAN_TAGGING
#undef JML_DISASSEMBLE
#define JML_TRACE_GC


#endif /* _JML_COMMON_H_ */
