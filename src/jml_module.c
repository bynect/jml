#include <stdio.h>

#include <jml.h>

#include <jml_module.h>
#include <jml_vm.h>
#include <jml_util.h>
#include <jml_gc.h>


#ifdef JML_PLATFORM_NIX

#include <dlfcn.h>
#include <sys/param.h>
#include <sys/types.h>
#include <limits.h>

#elif JML_PLATFORM_WIN

#include <windows.h>

#endif


void
jml_module_register(jml_obj_module_t *module, 
    jml_module_function *functions)
{
    jml_module_function *current = functions;

    while (current->function != NULL) {
        jml_cfunction_register(current->name,
            current->function, module); /*TODO*/

        ++current;
    }
}


jml_obj_module_t *
jml_module_open(jml_obj_string_t *module_name,
    char *path)
{
#ifdef JML_PLATFORM_NIX
    char *module_str = jml_strdup(module_name->chars);
    char filename_so[JML_PATH_MAX];
    char filename_jml[JML_PATH_MAX];

#ifdef JML_RECURSIVE_SEARCH
    char temp_so[256];
    char temp_jml[256];
    sprintf(temp_so, "%s.so", module_str);
    sprintf(temp_jml, "%s.jml", module_str);

    char buffer[JML_PATH_MAX];

    if (jml_file_find(temp_so, buffer))
        sprintf(filename_so, "%s", buffer);

    else {
        if (filename_so == NULL)
            sprintf(filename_so, "./%s.so", module_str);

        if (jml_file_find(temp_jml, buffer))
            sprintf(filename_jml, "%s", buffer);

        else
            sprintf(filename_jml, "./%s.jml", module_str);
    }

#else
    sprintf(filename_so, "./%s.so", module_str);
    sprintf(filename_jml, "./%s.jml", module_str);

#endif

    if (jml_file_exist(filename_so)) {
        void *handle = dlopen(filename_so, RTLD_NOW);

        if (handle == NULL) {
            jml_vm_error("ImportExc: %s.", dlerror());
            goto err;
        }

        strcpy(path, filename_so);

        jml_realloc(module_str, 0UL);
        return jml_obj_module_new(
            module_name->chars, handle);

    } else if (jml_file_exist(filename_jml)) {
        /*TODO*/
        strcpy(path, filename_jml);
        jml_vm_error("ImportExc: Cannot import jml module.");
        goto err;

    } else {
        jml_vm_error("ImportExc: Module not found.");
        goto err;
    }

    err: {
        jml_realloc(module_str, 0UL);
        return NULL;
    }

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
#ifdef JML_PLATFORM_NIX
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
#ifdef JML_PLATFORM_NIX
    jml_cfunction cfunction = dlsym(
        module->handle, function_name);

    char *result = dlerror();
    if (result || cfunction == NULL) {
        jml_vm_error("ImportExc%s.", result);
        return NULL;
    }

    return jml_obj_cfunction_new(
        function_name, cfunction, module);
#endif
}
