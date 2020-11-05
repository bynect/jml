#ifndef _JML_MODULE_H_
#define _JML_MODULE_H_

#include <jml_common.h>


#if defined (__linux__)                                 \
    || (defined (__unix__))                             \
    || (defined (__APPLE__)                             \
        && defined (__MACH__))

#define JML_IMPORT                  2

#elif defined(_WIN32)                                   \
    || defined(_WIN64)

#define JML_IMPORT                  1

#else

#define JML_IMPORT                  0

#endif


#endif /* _JML_MODULE_H_ */
