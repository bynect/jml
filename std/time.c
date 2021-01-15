#include <time.h>

#include <jml.h>


static jml_value_t
jml_std_time_time(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL((double)time(NULL));
}


static jml_value_t
jml_std_time_clock(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL((double)(clock() / CLOCKS_PER_SEC));
}


static jml_value_t
jml_std_time_localtime(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    time_t rawtime;
    time(&rawtime);

    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);

    char *local = asctime(timeinfo);
    return jml_string_intern(local);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"time",                        &jml_std_time_time},
    {"clock",                       &jml_std_time_clock},
    {"localtime",                   &jml_std_time_localtime},
    {NULL,                          NULL}
};
