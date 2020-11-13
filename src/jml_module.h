#ifndef JML_MODULE_H_
#define JML_MODULE_H_

#include <jml_common.h>
#include <jml_type.h>


typedef struct {
    const char                     *name;
    jml_cfunction                   function;
} jml_module_function;


void jml_module_register(jml_module_function *functions);


/*core module*/
void jml_core_register(void);


#endif /* JML_MODULE_H_ */
