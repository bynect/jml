#include <jml.h>

#include <jml_module.h>
#include <jml_vm.h>
#include <jml_util.h>


void
jml_module_register(jml_module_function *functions)
{
    int length = (sizeof(*functions) / sizeof(functions[0]));

    for (int i = 0; i < length; ++i) {
        jml_module_function current = functions[i];
        jml_cfunction_register(current.name, current.function);
    }
}

/*TODO*/
