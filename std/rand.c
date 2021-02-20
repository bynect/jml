#include <limits.h>
#include <stdlib.h>

#include <jml.h>


static jml_value_t
jml_std_rand_rand(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL(rand());
}


static jml_value_t
jml_std_rand_srand(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    jml_value_t val = args[0];

    if (!IS_NUM(val)) {
        exc = jml_core_exception_types(false, 1, "number");
        goto err;
    }

    double num = AS_NUM(val);
    if (num < 0 || num > UINT_MAX) {
        exc = jml_obj_exception_format(
            "OverlowErr",
            "Number exceed unsigned integer."
        );
        goto err;
    }

    srand((uint32_t)num);
    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"rand",                        &jml_std_rand_rand},
    {"srand",                       &jml_std_rand_srand},
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
    jml_module_add_value(module, "RAND_MAX", NUM_VAL(RAND_MAX));
}
