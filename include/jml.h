#ifndef _JML_H_
#define _JML_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#define JML_VERSION_MAJOR           0
#define JML_VERSION_MINOR           1
#define JML_VERSION_MICRO           0


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
#endif /* __cplusplus */

#endif /* _JML_H_ */
