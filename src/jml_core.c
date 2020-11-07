#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <jml.h>

#include <jml_module.h>
#include <jml_vm.h>
#include <jml_value.h>


static jml_obj_exception_t *
jml_core_exception_args(int arg_count, int expected_arg)
{
    char message[20];

    sprintf(message, "Expected %d got %d.", 
        expected_arg, arg_count);

    if (arg_count > expected_arg)
        return jml_obj_exception_new(
            "TooManyArgs", message);

    if (arg_count < expected_arg)
        return jml_obj_exception_new(
            "TooFewArgs", message);

    return NULL;
}


static jml_value_t
jml_core_clock(int arg_count, jml_value_t *args)
{

    jml_obj_exception_t *checked = jml_core_exception_args(
        arg_count, 0);

    if (checked != NULL)
        return OBJ_VAL(checked);

    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}


static jml_value_t
jml_core_print(int arg_count, jml_value_t *args)
{
    for (int i = 0; i < arg_count; i++) {
        jml_value_print(args[i]);
        printf("\n");
    }
    return NONE_VAL;
}


typedef struct {
    const char          *name;
    jml_cfunction       function;
} jml_core_function;


jml_core_function core_functions[] = {
    {"clock", &jml_core_clock},
    {"print", &jml_core_print},
};


void
jml_core_register(void)
{
    int length = (sizeof(core_functions) / sizeof(core_functions[0]));

    for (int i = 0; i < length; ++i) {
        jml_core_function current = core_functions[i];
        jml_cfunction_register(current.name, current.function);
    }
}
