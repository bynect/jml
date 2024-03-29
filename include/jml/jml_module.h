#ifndef JML_MODULE_H_
#define JML_MODULE_H_

#include <jml.h>

#include <jml/jml_common.h>
#include <jml/jml_type.h>


jml_obj_module_t *jml_module_open(jml_obj_string_t *qualified,
    jml_obj_string_t *name, jml_value_t *path);

bool jml_module_close(jml_obj_module_t *module);

jml_obj_cfunction_t *jml_module_get_raw(jml_obj_module_t *module,
    const char *name, bool silent);

void jml_module_register(jml_obj_module_t *module,
    jml_module_function *table);

bool jml_module_initialize(jml_obj_module_t *module);

void jml_module_finalize(jml_obj_module_t *module);


/*core module*/
void jml_core_register(jml_vm_t *vm);


#endif /* JML_MODULE_H_ */
