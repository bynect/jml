#include <stdio.h>


#include <jml.h>


#define BIT_FUNC2(name, op)                             \
    static jml_value_t                                  \
    name(int arg_count, jml_value_t *args)              \
    {                                                   \
        jml_obj_exception_t *exc = jml_error_args(      \
            arg_count, 2);                              \
                                                        \
        if (exc != NULL)                                \
            goto err;                                   \
                                                        \
        if (!IS_NUM(args[0]) || !IS_NUM(args[1])) {     \
            exc = jml_error_types(                      \
                false, 2, "number", "number"            \
            );                                          \
            goto err;                                   \
        }                                               \
                                                        \
        double num1 = AS_NUM(args[0]);                  \
        double num2 = AS_NUM(args[1]);                  \
                                                        \
        if ((num1 >= INT64_MAX || num1 < INT64_MIN)     \
            ||(num2 >= INT64_MAX || num2 < INT64_MIN)) {\
                                                        \
            exc = jml_error_value("number");            \
            goto err;                                   \
        }                                               \
                                                        \
        return NUM_VAL((int64_t)num1 op (int64_t)num2); \
                                                        \
    err:                                                \
        return OBJ_VAL(exc);                            \
    }


BIT_FUNC2(jml_std_bit_lshift, <<)
BIT_FUNC2(jml_std_bit_rshift, >>)
BIT_FUNC2(jml_std_bit_and, &)
BIT_FUNC2(jml_std_bit_or, |)
BIT_FUNC2(jml_std_bit_xor, ^)


static jml_value_t
jml_std_bit_not(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0])) {
        exc = jml_error_types(false, 1, "number");
        goto err;
    }

    double num = AS_NUM(args[0]);

    if ((num >= INT64_MAX || num < INT64_MIN)) {
        exc = jml_error_value("number");
        goto err;
    }

    return NUM_VAL(~(int64_t)num);

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"lshift",                      &jml_std_bit_lshift},
    {"rshift",                      &jml_std_bit_rshift},
    {"_and",                        &jml_std_bit_and},
    {"_or",                         &jml_std_bit_or},
    {"xor",                         &jml_std_bit_xor},
    {"_not",                        &jml_std_bit_not},
    {NULL,                          NULL}
};
