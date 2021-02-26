#include <string.h>
#include <stdio.h>

#include <jml.h>


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#include <regex.h>

#else

#error Current platform not supported.

#endif


static regex_t                      last_rule;
static int                          last_result = -1;
static char                        *last_string = NULL;


#define REGEX_CHECK(exc, arg_count, args, arg_num, ...) \
    do {                                                \
        exc = jml_error_args(                           \
            arg_count, arg_num);                        \
        if (exc != NULL)                                \
            goto err;                                   \
                                                        \
        if (!IS_STRING(args[0])                         \
            || !IS_STRING(args[1])) {                   \
            exc = jml_error_types(                      \
                false, arg_num, __VA_ARGS__);           \
            goto err;                                   \
        }                                               \
    } while (false)


#define REGEX_REUSE(rule, result)                       \
    do {                                                \
        if (last_string == NULL || strncmp(last_string, \
            rule->chars, rule->length) != 0) {          \
                                                        \
            if ((result = regcomp(&last_rule,           \
                rule->chars, REG_EXTENDED)) != 0) {     \
                last_result = result;                   \
                goto regerr;                            \
            } else {                                    \
                last_result = result;                   \
                last_string = rule->chars;              \
            }                                           \
        }                                               \
    } while (false)


#define REGEX_ERR(exc, result)                          \
    do {                                                \
        regerr: {                                       \
            char buffer[4096];                          \
            regerror(result, &last_rule, buffer, 4096); \
                                                        \
            exc = jml_obj_exception_new(                \
                "RegexErr", buffer);                    \
                                                        \
            regfree(&last_rule);                        \
            goto err;                                   \
        }                                               \
                                                        \
        err: {                                          \
            return OBJ_VAL(exc);                        \
        }                                               \
    } while (false)


static jml_value_t
jml_std_regex_match(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc;
    int result;

    REGEX_CHECK(exc, arg_count, args, 2, "string", "string");

    jml_obj_string_t *rule          = AS_STRING(args[0]);
    jml_obj_string_t *string        = AS_STRING(args[1]);

    REGEX_REUSE(rule, result);

    result = regexec(&last_rule, string->chars, 0, NULL, 0);

    if (!result)                    return TRUE_VAL;
    else if (result == REG_NOMATCH) return FALSE_VAL;
    else                            goto regerr;

    REGEX_ERR(exc, result);
}


static jml_value_t
jml_std_regex_search(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc;
    int result;
    REGEX_CHECK(exc, arg_count, args, 2, "string", "string");

    jml_obj_string_t *rule          = AS_STRING(args[0]);
    jml_obj_string_t *string        = AS_STRING(args[1]);

    REGEX_REUSE(rule, result);

    int                 max_match   = 16;
    int                 max_group   = 16;

    regmatch_t         *matches     = jml_alloc(max_match * sizeof(regmatch_t));
    char               *copy        = jml_strdup(string->chars);
    char               *current     = copy;

    jml_obj_array_t *array          = jml_obj_array_new();
    jml_value_t      array_value    = OBJ_VAL(array);
    jml_gc_exempt(array_value);

    int i = 0;
    for ( ; i < max_match; ++i) {

        if (max_match <= i + 1) {
            max_match *= 1.5;
            matches = jml_realloc(matches, max_match * sizeof(regmatch_t));
        }

        if ((result = regexec(&last_rule, current, max_match, matches, 0)))
            break;

        int groups  = 0;
        int offset  = 0;

        for (groups = 0; groups < max_group; ++groups) {
            if (matches[groups].rm_so == -1)
                break;

            if (groups == 0)
                offset = matches[groups].rm_eo;

            if (max_group <= groups + 1)
                max_group *= 1.5;

            int index = matches[groups].rm_eo - matches[groups].rm_so;

            jml_obj_array_append(array, OBJ_VAL(
                jml_obj_string_copy(current + matches[groups].rm_so, index)
            ));
        }
        current += offset;
    }

    jml_free(copy);
    jml_free(matches);

    jml_gc_unexempt(array_value);
    return OBJ_VAL(array);

    REGEX_ERR(exc, result);
}


#undef REGEX_CHECK
#undef REGEX_REUSE
#undef REGEX_ERR


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"match",                       &jml_std_regex_match},
    {"search",                      &jml_std_regex_search},
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(JML_UNUSED(jml_obj_module_t *module))
{
    memset(&last_rule, 0, sizeof(regex_t));
}


MODULE_FUNC_HEAD
module_free(JML_UNUSED(jml_obj_module_t *module))
{
    if (!last_result)
        regfree(&last_rule);
}
