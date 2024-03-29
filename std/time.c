#ifdef __GNUC__

#define _XOPEN_SOURCE               500

#endif

#include <jml.h>


#if defined JML_PLATFORM_NIX

#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#elif defined JML_PLATFORM_WIN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>

#endif


static jml_value_t
jml_std_time_sleep(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0])) {
        exc = jml_error_types(
            false, 1, "number"
        );
        goto err;
    }

#if defined JML_PLATFORM_NIX

    sleep((uint32_t)AS_NUM(args[0]));

#elif defined JML_PLATFORM_WIN

    Sleep((uint32_t)AS_NUM(args[0]) * 1000);

#endif

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_time_usleep(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0])) {
        exc = jml_error_types(
            false, 1, "number"
        );
        goto err;
    }

#if defined JML_PLATFORM_NIX

    usleep((uint64_t)AS_NUM(args[0]));

#elif defined JML_PLATFORM_WIN

    Sleep((uint32_t)AS_NUM(args[0]));

#endif

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_time_time(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL((double)time(NULL));
}


static jml_value_t
jml_std_time_clock(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL((double)(clock() / CLOCKS_PER_SEC));
}


static jml_value_t
jml_std_time_difftime(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0]) || !IS_NUM(args[1])) {
        exc = jml_error_types(
            false, 2, "number", "number"
        );
        goto err;
    }

    return NUM_VAL(
        difftime((time_t)AS_NUM(args[0]), (time_t)AS_NUM(args[1]))
    );

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_time_localtime(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    time_t rawtime;
    time(&rawtime);

    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);

    char *local = asctime(timeinfo);
    size_t length = strlen(local) - 1;
    local[length] = '\0';

    return OBJ_VAL(
        jml_obj_string_copy(local, length)
    );
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"sleep",                       &jml_std_time_sleep},
    {"usleep",                      &jml_std_time_usleep},
    {"time",                        &jml_std_time_time},
    {"clock",                       &jml_std_time_clock},
    {"difftime",                    &jml_std_time_difftime},
    {"localtime",                   &jml_std_time_localtime},
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
    jml_module_add_value(module, "CLOCKS_PER_SEC", NUM_VAL(CLOCKS_PER_SEC));
}
