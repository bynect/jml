#ifndef JML_VM_H_
#define JML_VM_H_

#include <jml_value.h>
#include <jml_type.h>
#include <jml_bytecode.h>


typedef struct {
    jml_obj_closure_t              *closure;
    uint8_t                        *pc;
    jml_value_t                    *slots;
} jml_call_frame_t;


typedef struct jml_vm_s {
    jml_call_frame_t                frames[FRAMES_MAX];
    uint8_t                         frame_count;
    jml_obj_upvalue_t              *open_upvalues;

    jml_value_t                     stack[STACK_MAX];
    jml_value_t                    *stack_top;
    jml_hashmap_t                   globals;
    jml_hashmap_t                   strings;
    jml_hashmap_t                   modules;

    jml_obj_string_t               *init_string;
    jml_obj_string_t               *call_string;
    jml_obj_string_t               *module_string;
    jml_obj_string_t               *path_string;
    jml_obj_string_t               *external;

    jml_obj_t                      *sentinel;
    jml_obj_t                      *objects;
    size_t                          allocated;
    size_t                          next_gc;
    int                             gray_count;
    int                             gray_capacity;
    jml_obj_t                     **gray_stack;

    jml_value_t                     exempt_stack[EXEMPT_MAX];
    jml_value_t                    *exempt;
} jml_vm_t;


void jml_vm_init(jml_vm_t *vm);

void jml_vm_push(jml_value_t value);

jml_value_t jml_vm_pop();

void jml_vm_pop_two(void);

jml_value_t jml_vm_peek(int distance);

void jml_vm_error(const char *format, ...);


void jml_cfunction_register(const char *name,
    jml_cfunction function, jml_obj_module_t *module);


extern jml_vm_t *vm;


#endif /* JML_VM_H_ */
