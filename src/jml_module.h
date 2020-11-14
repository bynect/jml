#ifndef JML_MODULE_H_
#define JML_MODULE_H_

#include <jml_common.h>
#include <jml_type.h>


typedef struct {
    const char                     *name;
    jml_cfunction                   function;
} jml_module_function;


void jml_module_register(jml_module_function *functions);

jml_obj_module_t *jml_module_open(jml_obj_string_t *module_name);

bool jml_module_close(jml_obj_module_t *module);

jml_obj_cfunction_t *jml_module_get_raw(jml_obj_module_t *module,
    const char *function_name);


/*core module*/
void jml_core_register(void);


#endif /* JML_MODULE_H_ */
