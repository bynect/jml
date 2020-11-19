#include <regex.h>
#include <string.h>

#include <jml.h>


static regex_t                     *last_rule;
static char                        *last_string;


static jml_value_t
jml_std_regex_match(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc;

    exc = jml_core_exception_args(arg_count, 2);
    if (exc != NULL) goto err;

    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        exc = jml_core_exception_types(false, 2, "string", "string");
        goto err;
    }

    jml_obj_string_t *rule          = AS_STRING(args[0]);
    jml_obj_string_t *string        = AS_STRING(args[1]);

    if (last_string == NULL || strncmp(last_string,
        rule->chars, rule->length) != 0) {

        last_string = rule->chars;
        if (regcomp(last_rule, rule->chars, REG_EXTENDED)) {
            exc = jml_obj_exception_new(
                "RegexExc", "Regex failed"
            );
            goto err;
        }
    }

    int result = regexec(last_rule, string->chars, 0, NULL, 0);

    if (!result)                    return TRUE_VAL;
    else if (result == REG_NOMATCH) return FALSE_VAL;
    else {
        exc = jml_obj_exception_new(
            "RegexExc", "Regex failed"
        );
        goto err;
    }

    err: {
        if (exc != NULL)
            return OBJ_VAL(exc);
    }

    return NONE_VAL;
}


jml_module_function module_table[] = {
    {"match",                       &jml_std_regex_match},
    {NULL,                          NULL}
};


void
module_init(jml_obj_module_t *module)
{
    last_rule = jml_realloc(NULL, sizeof(regex_t));
    memset(last_rule, 0, sizeof(regex_t));

    last_string = NULL;
}


void
module_free(jml_obj_module_t *module)
{
    regfree(last_rule);
}
