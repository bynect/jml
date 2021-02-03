#ifdef __GNUC__
#define _XOPEN_SOURCE               500
#elif defined _MSC_VER
#define _USE_MATH_DEFINES
#endif

#include <math.h>

#include <jml.h>


#define MATH_NAME(func)             jml_std_math_ ## func


#define MATH_CHECK(arg_count, args, arg_num, ...)       \
    do {                                                \
        exc = jml_core_exception_args(                  \
            arg_count, arg_num);                        \
        if (exc != NULL) goto err;                      \
                                                        \
        for (int i = 0; i < arg_count; ++i)             \
            if (!IS_NUM(args[i])) {                     \
                exc = jml_core_exception_types(         \
                false, arg_num, __VA_ARGS__);           \
                goto err;                               \
            }                                           \
    } while (false)


#define MATH_ERR(exc)                                   \
    do {                                                \
        err:                                            \
            return OBJ_VAL(exc);                        \
    } while (false)


#define MATH_FUNC1(func)                                \
    static jml_value_t                                  \
    MATH_NAME(func)(int arg_count, jml_value_t *args)   \
    {                                                   \
        jml_obj_exception_t *exc;                       \
                                                        \
        MATH_CHECK(arg_count, args, 1, "number");       \
                                                        \
        return NUM_VAL(func(                            \
            AS_NUM(args[0])                             \
        ));                                             \
                                                        \
        MATH_ERR(exc);                                  \
    }


#define MATH_FUNC2(func)                                \
    static jml_value_t                                  \
    MATH_NAME(func)(int arg_count, jml_value_t *args)   \
    {                                                   \
        jml_obj_exception_t *exc;                       \
                                                        \
        MATH_CHECK(arg_count, args, 2, "number");       \
                                                        \
        return NUM_VAL(func(                            \
            AS_NUM(args[0]),                            \
            AS_NUM(args[1])                             \
        ));                                             \
                                                        \
        MATH_ERR(exc);                                  \
    }


#define MATH_FUNC3(func)                                \
    static jml_value_t                                  \
    MATH_NAME(func)(int arg_count, jml_value_t *args)   \
    {                                                   \
        jml_obj_exception_t *exc;                       \
                                                        \
        MATH_CHECK(arg_count, args, 3, "number");       \
                                                        \
        return NUM_VAL(func(                            \
            AS_NUM(args[0]),                            \
            AS_NUM(args[1]),                            \
            AS_NUM(args[2])                             \
        ));                                             \
                                                        \
        MATH_ERR(exc);                                  \
    }


MATH_FUNC1(acos)
MATH_FUNC1(acosh)
MATH_FUNC1(asin)
MATH_FUNC1(asinh)
MATH_FUNC1(atan)
MATH_FUNC2(atan2)
MATH_FUNC1(atanh)
MATH_FUNC1(cbrt)
MATH_FUNC1(ceil)
MATH_FUNC2(copysign)
MATH_FUNC1(cos)
MATH_FUNC1(cosh)
MATH_FUNC1(erf)
MATH_FUNC1(erfc)
MATH_FUNC1(exp)
MATH_FUNC1(exp2)
MATH_FUNC1(expm1)
MATH_FUNC1(fabs)
MATH_FUNC2(fdim)
MATH_FUNC1(floor)
MATH_FUNC3(fma)
MATH_FUNC2(fmax)
MATH_FUNC2(fmin)
MATH_FUNC2(fmod)
MATH_FUNC2(hypot)
MATH_FUNC1(lgamma)
MATH_FUNC1(log)
MATH_FUNC1(log10)
MATH_FUNC1(log1p)
MATH_FUNC1(log2)
MATH_FUNC1(logb)
MATH_FUNC1(nearbyint)
MATH_FUNC2(nextafter)
MATH_FUNC2(nexttoward)
MATH_FUNC2(pow)
MATH_FUNC2(remainder)
MATH_FUNC1(rint)
MATH_FUNC1(round)
MATH_FUNC1(sin)
MATH_FUNC1(sinh)
MATH_FUNC1(sqrt)
MATH_FUNC1(tan)
MATH_FUNC1(tanh)
MATH_FUNC1(tgamma)
MATH_FUNC1(trunc)


#undef MATH_CHECK
#undef MATH_ERR
#undef MATH_FUNC1
#undef MATH_FUNC2
#undef MATH_FUNC3


/*module table*/
#define MATH_ENTRY(func)                                \
    {#func,                         &MATH_NAME(func)}


MODULE_TABLE_HEAD module_table[] = {
    MATH_ENTRY(acos),
    MATH_ENTRY(acosh),
    MATH_ENTRY(asin),
    MATH_ENTRY(asinh),
    MATH_ENTRY(atan),
    MATH_ENTRY(atan2),
    MATH_ENTRY(atanh),
    MATH_ENTRY(cbrt),
    MATH_ENTRY(ceil),
    MATH_ENTRY(copysign),
    MATH_ENTRY(cos),
    MATH_ENTRY(cosh),
    MATH_ENTRY(erf),
    MATH_ENTRY(erfc),
    MATH_ENTRY(exp),
    MATH_ENTRY(exp2),
    MATH_ENTRY(expm1),
    MATH_ENTRY(fabs),
    MATH_ENTRY(fdim),
    MATH_ENTRY(floor),
    MATH_ENTRY(fma),
    MATH_ENTRY(fmax),
    MATH_ENTRY(fmin),
    MATH_ENTRY(fmod),
    MATH_ENTRY(hypot),
    MATH_ENTRY(lgamma),
    MATH_ENTRY(log),
    MATH_ENTRY(log10),
    MATH_ENTRY(log1p),
    MATH_ENTRY(log2),
    MATH_ENTRY(logb),
    MATH_ENTRY(nearbyint),
    MATH_ENTRY(nextafter),
    MATH_ENTRY(nexttoward),
    MATH_ENTRY(pow),
    MATH_ENTRY(remainder),
    MATH_ENTRY(rint),
    MATH_ENTRY(round),
    MATH_ENTRY(sin),
    MATH_ENTRY(sinh),
    MATH_ENTRY(sqrt),
    MATH_ENTRY(tan),
    MATH_ENTRY(tanh),
    MATH_ENTRY(tgamma),
    MATH_ENTRY(trunc),
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
#if defined __GNUC__ || defined _MSC_VER
    jml_module_add_value(module, "e",       NUM_VAL(M_E));
    jml_module_add_value(module, "log2e",   NUM_VAL(M_LOG2E));
    jml_module_add_value(module, "log10e",  NUM_VAL(M_LOG10E));
    jml_module_add_value(module, "ln2",     NUM_VAL(M_LN2));
    jml_module_add_value(module, "ln10",    NUM_VAL(M_LN10));
    jml_module_add_value(module, "pi",      NUM_VAL(M_PI));
#endif

#ifndef NAN

#define NAN                         -(0.f/0.f)

#endif
    jml_module_add_value(module, "nan",     NUM_VAL(NAN));

#ifndef INFINITY

#define INFINITY                    HUGE_VAL

#endif
    jml_module_add_value(module, "inf",     NUM_VAL(INFINITY));
}
