#include <jml.h>

#include <jml_module.h>
#include <jml_vm.h>
#include <jml_util.h>
#include <jml_gc.h>
#include <stdio.h>


#if JML_PLATFORM >= 1 && JML_PLATFORM < 5

#include <dlfcn.h>

#endif


void
jml_module_register(jml_module_function *functions)
{
    jml_module_function *current = functions;

    while (current->function != NULL) {
        jml_cfunction_register(current->name,
            current->function, NULL); /*TODO*/

        ++current;
    }
}


/*TODO*/
jml_obj_module_t *
jml_module_open(jml_obj_string_t *module_name)
{
#if JML_PLATFORM >= 1 && JML_PLATFORM < 5

    char *module_str = jml_strdup(module_name->chars);
    char filename[256];
    sprintf(filename, "./%s.so", module_str);

    void *extension = dlopen(filename, RTLD_NOW);

    if (!extension) {
        jml_realloc(module_str, 0UL);

        jml_vm_error("ImportExc: %s.", dlerror());
        return NULL;
    }

    jml_realloc(module_str, 0UL);

    return jml_obj_module_new(
        module_name->chars, extension);
#else
    jml_vm_error(
        "Extension import not supported on %s",
        JML_PLATFORM_STRING
    );
    return NULL;
#endif
}


bool
jml_module_close(jml_obj_module_t *module)
{
#if JML_PLATFORM >= 1 && JML_PLATFORM < 5

    if (module == NULL || module->handle == NULL)
        return false;

    dlclose(module->handle);
    return true;
#endif
}


jml_obj_cfunction_t *
jml_module_get_raw(jml_obj_module_t *module,
    const char *function_name)
{
#if JML_PLATFORM >= 1 && JML_PLATFORM < 5

    jml_cfunction cfunction = dlsym(
        module->handle, function_name);

    char *result = dlerror();
    if (result || cfunction == NULL) {
        jml_vm_error("ImportExc%s.", result);
        return NULL;
    }

    return jml_obj_cfunction_new(
        function_name, cfunction, module);
#else

#endif
}
