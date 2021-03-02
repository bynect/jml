#ifndef JML_VM_H_
#define JML_VM_H_

#include <jml_value.h>
#include <jml_type.h>
#include <jml_compiler.h>


struct jml_vm {
    jml_call_frame_t                frames[FRAMES_MAX];
    uint8_t                         frame_count;
    jml_obj_upvalue_t              *open_upvalues;

    jml_value_t                     stack[STACK_MAX];
    jml_value_t                     cstack[STACK_MAX];
    jml_value_t                    *stack_top;

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

    jml_obj_module_t               *current;
    jml_obj_cfunction_t            *external;

    jml_obj_t                      *sentinel;
    jml_obj_t                      *objects;
    size_t                          allocated;
    size_t                          next_gc;
    int64_t                         gray_count;
    int64_t                         gray_capacity;
    jml_obj_t                     **gray_stack;

    jml_compiler_t                 *compilers[4];
    jml_compiler_t                 **compiler_top;
};


void jml_vm_init(jml_vm_t *vm);

void jml_vm_reset(jml_vm_t *vm);

void jml_vm_push(jml_value_t value);

jml_value_t jml_vm_pop(void);

jml_value_t jml_vm_pop_two(void);

void jml_vm_rot(void);

jml_value_t jml_vm_peek(int distance);

void jml_vm_error(const char *format, ...);

bool jml_vm_call_value(jml_value_t callee, int arg_count);

bool jml_vm_call_cstack(jml_value_t callee, int arg_count,
    jml_value_t *last);

bool jml_vm_invoke_cstack(jml_obj_instance_t *instance,
    jml_obj_string_t *name, int arg_count, jml_value_t *last);

void jml_cfunction_register(const char *name,
    jml_cfunction function, jml_obj_module_t *module);


extern jml_vm_t *vm;


#endif /* JML_VM_H_ */
