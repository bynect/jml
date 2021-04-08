#ifndef JML_REPR_H_
#define JML_REPR_H_

#include <jml.h>


void jml_value_print(jml_value_t value);

char *jml_value_stringify(jml_value_t value);

const char *jml_value_stringify_type(jml_value_t value);


void jml_obj_print(jml_value_t value);

char *jml_obj_stringify(jml_value_t value);

const char *jml_obj_stringify_type(jml_value_t value);


#endif /* JML_REPR_H_ */
