#ifndef _JML_VM_H_
#define _JML_VM_H_

#include <jml_common.h>
#include <jml_value.h>
#include <jml_type.h>
#include <jml_gc.h>

#define FRAMES_MAX                  64
#define STACK_MAX                   (FRAMES_MAX * LOCAL_MAX)


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
    jml_obj_upvalue_t              *open_upvalues;

    jml_gc_t                        gc;
    jml_vm_status                   status;
} jml_vm_t;


typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} jml_interpret_result;


typedef enum {
    VM_INITIALIZED,
    VM_FREED,
    VM_ERROR
} jml_vm_status;


void jml_vm_init(void);
void jml_vm_free(void);

jml_interpret_result interpret(const char *source);

void jml_vm_push(jml_value_t value);
jml_value_t jml_vm_pop();


#endif /* _JML_VM_H_ */
