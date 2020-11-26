#include <regex.h>
#include <string.h>
#include <stdio.h>

#include <jml.h>


static regex_t                     *last_rule;
static char                        *last_string;


#define REGEX_CHECK(exc, arg_count, args, arg_num, ...) \
    do {                                                \
        exc = jml_core_exception_args(                  \
            arg_count, arg_num);                        \
        if (exc != NULL) goto err;                      \
                                                        \
        if (!IS_STRING(args[0])                         \
            || !IS_STRING(args[1])) {                   \
            exc = jml_core_exception_types(             \
                false, arg_num, __VA_ARGS__);           \
            goto err;                                   \
        }                                               \
    } while (false)

#define REGEX_REUSE(rule, result)                       \
    do {                                                \
        if (last_string == NULL || strncmp(last_string, \
            rule->chars, rule->length) != 0) {          \
                                                        \
            last_string = rule->chars;                  \
            if ((result = regcomp(last_rule,            \
                rule->chars, REG_EXTENDED)) != 0)       \
                goto regerr;                            \
        }                                               \
    } while (false)

#define REGEX_ERR()                                     \
    do {                                                \
        regerr: {                                       \
            size_t size = 2048;                         \
            char buff[size];                            \
            regerror(result, last_rule, buff, size);    \
            exc = jml_obj_exception_new(                \
                "RegexExc", buff);                      \
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

    result = regexec(last_rule, string->chars, 0, NULL, 0);

    if (!result)                    return TRUE_VAL;
    else if (result == REG_NOMATCH) return FALSE_VAL;
    else                            goto regerr;

    REGEX_ERR();
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

    regmatch_t         *matches     = jml_realloc(NULL, max_match * sizeof(regmatch_t));
    char               *copy        = jml_strdup(string->chars);
    char               *current     = copy;

    jml_obj_array_t *array = jml_obj_array_new();
    jml_gc_exempt_push(OBJ_VAL(array));

    int i = 0;
    for ( ; i < max_match; ++i) {

        if (max_match <= i + 1) {
            max_match *= 1.5;
            matches = jml_realloc(matches, max_match * sizeof(regmatch_t));
        }

        if ((result = regexec(last_rule, current, max_match, matches, 0)))
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

            jml_obj_array_add(array, OBJ_VAL(
                jml_obj_string_copy(current + matches[groups].rm_so, index)
            ));
        }
        current += offset;
    }

    jml_realloc(copy, 0UL);
    jml_realloc(matches, 0UL);

    jml_gc_exempt_pop();
    return OBJ_VAL(array);

    REGEX_ERR();
}


#undef REGEX_CHECK
#undef REGEX_REUSE
#undef REGEX_ERR


/*module table*/
MODULE_TABLE_HEAD = {
    {"match",                       &jml_std_regex_match},
    {"search",                      &jml_std_regex_search},
    {NULL,                          NULL}
};


void
module_init(JML_UNUSED(jml_obj_module_t *module))
{
    last_string = NULL;

    last_rule   = jml_realloc(NULL, sizeof(regex_t));
    memset(last_rule, 0, sizeof(regex_t));
}


void
module_free(JML_UNUSED(jml_obj_module_t *module))
{
    if (last_rule != NULL)
        regfree(last_rule);
}
