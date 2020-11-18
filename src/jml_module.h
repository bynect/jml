#ifndef JML_MODULE_H_
#define JML_MODULE_H_

#include <jml.h>

#include <jml_common.h>
#include <jml_type.h>


jml_obj_module_t *jml_module_open(jml_obj_string_t *module_name, char *path);

bool jml_module_close(jml_obj_module_t *module);

jml_obj_cfunction_t *jml_module_get_raw(jml_obj_module_t *module,
    const char *function_name, bool silent);

jml_module_function *jml_module_get_table(jml_obj_module_t *module,
    bool silent);

void jml_module_register(jml_obj_module_t *module,
    jml_module_function *function_table);

bool jml_module_initialize(jml_obj_module_t *module);

void jml_module_finalize(jml_obj_module_t *module);


/*core module*/
void jml_core_register(void);


jml_obj_exception_t *jml_core_exception_args(
    int arg_count, int expected_arg);

jml_obj_exception_t *jml_core_exception_implemented(
    jml_value_t value);

jml_obj_exception_t *jml_core_exception_types(
    bool mult, int arg_count, ...);


#endif /* JML_MODULE_H_ */
