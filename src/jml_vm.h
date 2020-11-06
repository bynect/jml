#ifndef _JML_VM_H_
#define _JML_VM_H_

#include <jml_common.h>
#include <jml_value.h>
#include <jml_type.h>
#include <jml_gc.h>


#define FRAMES_MAX                  64
#define STACK_MAX                   (FRAMES_MAX * LOCAL_MAX)


typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} jml_interpret_result;


typedef struct {
    jml_obj_closure_t              *closure;
    uint8_t                        *pc;
    jml_value_t                    *slots;
} jml_call_frame_t;


typedef struct {
    jml_call_frame_t                frames[FRAMES_MAX];
    uint8_t                         frame_count;

    jml_value_t                     stack[STACK_MAX];
    jml_value_t                    *stack_top;
    jml_hashmap_t                   globals;
    jml_hashmap_t                   strings;
    jml_obj_string_t               *init_string;
    jml_obj_string_t               *call_string;
    jml_obj_upvalue_t              *open_upvalues;

    size_t                          allocated;
    size_t                          next_gc;
    jml_obj_t                      *objects;
    int                             gray_count;
    int                             gray_capacity;
    jml_obj_t                     **gray_stack;
} jml_vm_t;


jml_vm_t *jml_vm_new(void);

void jml_vm_init(jml_vm_t *vm);

void jml_vm_free(jml_vm_t *vm);

jml_interpret_result jml_vm_interpret(const char *source);

void jml_vm_push(jml_value_t value);

jml_value_t jml_vm_pop();


extern jml_vm_t *vm;


#endif /* _JML_VM_H_ */
