#include <jml.h>


static jml_value_t
jml_std_bytecode_disassemble(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    jml_obj_function_t *function;

    if (IS_CLOSURE(args[0]))
        function = AS_CLOSURE(args[0])->function;

    else if (IS_FUNCTION(args[0]))
        function = AS_FUNCTION(args[0]);

    else {
        exc = jml_error_types(false, 1, "function");
        goto err;
    }

    jml_bytecode_disassemble(
        &function->bytecode, function->name->chars
    );

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"disassemble",                 &jml_std_bytecode_disassemble},
    {NULL,                          NULL}
};
