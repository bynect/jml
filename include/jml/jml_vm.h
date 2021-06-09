#ifndef JML_VM_H_
#define JML_VM_H_

#include <jml/jml_value.h>
#include <jml/jml_type.h>
#include <jml/jml_compiler.h>


#ifdef JML_VM_INTERNAL

struct jml_vm {
    jml_vm_context_t               *context;

    jml_obj_coroutine_t            *running;
    jml_obj_module_t               *current;
    jml_obj_cfunction_t            *external;

    jml_obj_t                      *objects;
    size_t                          allocated;
    size_t                          next_gc;
    int64_t                         gray_count;
    int64_t                         gray_capacity;
    jml_obj_t                     **gray_stack;

    jml_compiler_t                 *compilers[4];
    jml_compiler_t                 **compiler_top;

    jml_value_t                     exempt_stack[EXEMPT_MAX];
    jml_value_t                    *exempt_top;

    jml_hashmap_t                   globals;
    jml_hashmap_t                   strings;
    jml_hashmap_t                   modules;
    jml_hashmap_t                   builtins;

    jml_obj_string_t               *main_string;
    jml_obj_string_t               *module_string;
    jml_obj_string_t               *path_string;
    jml_obj_string_t               *init_string;
    jml_obj_string_t               *call_string;
    jml_obj_string_t               *free_string;
    jml_obj_string_t               *add_string;
    jml_obj_string_t               *sub_string;
    jml_obj_string_t               *mul_string;
    jml_obj_string_t               *pow_string;
    jml_obj_string_t               *div_string;
    jml_obj_string_t               *mod_string;
    jml_obj_string_t               *gt_string;
    jml_obj_string_t               *ge_string;
    jml_obj_string_t               *lt_string;
    jml_obj_string_t               *le_string;
    jml_obj_string_t               *concat_string;
    jml_obj_string_t               *get_string;
    jml_obj_string_t               *set_string;
    jml_obj_string_t               *size_string;
    jml_obj_string_t               *print_string;
    jml_obj_string_t               *str_string;
    jml_obj_string_t               *inherit_string;
};

#endif


void jml_vm_init(jml_vm_t *vm, jml_vm_context_t *context);

void jml_vm_error(const char *format, ...);

bool jml_vm_call_value(jml_obj_coroutine_t *coroutine,
    jml_value_t callee, int arg_count);

jml_interpret_result jml_vm_call_coroutine(
    jml_obj_coroutine_t *coroutine, jml_value_t *last);

void jml_cfunction_register(const char *name,
    jml_cfunction function, jml_obj_module_t *module);


extern jml_vm_t *vm;


#endif /* JML_VM_H_ */
