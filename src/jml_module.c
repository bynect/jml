#include <stdio.h>

#include <jml_module.h>
#include <jml_vm.h>
#include <jml_util.h>
#include <jml_gc.h>
#include <jml_compiler.h>


#ifdef JML_PLATFORM_NIX

#include <dlfcn.h>
#include <sys/param.h>
#include <sys/types.h>
#include <limits.h>

#ifdef JML_PLATFORM_MAC
#define SHARED_LIB_EXT              "dylib"
#else
#define SHARED_LIB_EXT              "so"
#endif

#elif JML_PLATFORM_WIN

#include <windows.h>

#define SHARED_LIB_EXT              "dll"

#endif


jml_obj_module_t *
jml_module_open(jml_obj_string_t *module_name,
    char *path)
{
#ifdef JML_PLATFORM_NIX
    char *module_str = jml_strdup(
        module_name->chars);

    char filename_so[JML_PATH_MAX];
    char filename_jml[JML_PATH_MAX];

#ifdef JML_RECURSIVE_SEARCH
    char temp_so[256];
    char temp_jml[256];
    char buffer[JML_PATH_MAX] = { 0 };

    sprintf(temp_so, "%s.%s", module_str, SHARED_LIB_EXT);
    sprintf(temp_jml, "%s.jml", module_str);

    if (jml_file_find(temp_so, buffer))
        sprintf(filename_so, "%s", buffer);

    else if (jml_file_find(temp_jml, buffer))
        sprintf(filename_jml, "%s", buffer);
    
    else {
        jml_vm_error("ImportExc: Module not found.");
        goto err;
    }
#else
    sprintf(filename_so, "./%s.%s", module_str, SHARED_LIB_EXT);
    sprintf(filename_jml, "./%s.jml", module_str);
#endif

    if (jml_file_exist(filename_so)) {
        void *handle = dlopen(filename_so, RTLD_NOW);

        if (handle == NULL) {
            jml_vm_error("ImportExc: %s.", dlerror());
            goto err;
        }

        if (path != NULL)
            strcpy(path, filename_so);

        jml_realloc(module_str, 0UL);

        return jml_obj_module_new(
            module_name, handle);

    } else if (jml_file_exist(filename_jml)) {
        /*TODO*/
        if (path != NULL)
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
    const char *function_name, bool silent)
{
#ifdef JML_PLATFORM_NIX
    jml_cfunction raw = dlsym(
        module->handle, function_name);

    char *result = dlerror();
    if (result || raw == NULL) {
        if (!silent)
            jml_vm_error("ImportExc: %s.", result);

        return NULL;
    }

    jml_vm_push(jml_string_intern(function_name));

    jml_obj_cfunction_t *cfunction = jml_obj_cfunction_new(
        AS_STRING(jml_vm_peek(0)), raw, module);

    jml_vm_pop();

    return cfunction;
#endif
}


void
jml_module_register(jml_obj_module_t *module,
    jml_module_function *function_table)
{
    jml_module_function *current = function_table;

    while (current->function != NULL) {
        jml_vm_push(jml_string_intern(current->name));

        jml_vm_push(OBJ_VAL(
            jml_obj_cfunction_new(AS_STRING(jml_vm_peek(0)),
                current->function, module)
        ));

        jml_hashmap_set(&module->globals,
            AS_STRING(jml_vm_peek(0)),
            jml_vm_peek(1)
        );

        jml_vm_pop_two();
        ++current;
    }
}


bool
jml_module_initialize(jml_obj_module_t *module)
{
#ifdef JML_PLATFORM_NIX
    if (module->handle == NULL)
        return true;

    jml_mfunction initializer = dlsym(
        module->handle, "module_init"
    );

    if (initializer != NULL) {
        initializer(module);
    }

#ifndef JML_LAZY_IMPORT
    jml_module_function *table = dlsym(
        module->handle, "module_table"
    );

    if (table == NULL) return false;
    jml_module_function *current = table;

    while (current->function != NULL) {
        jml_vm_push(jml_string_intern(current->name));

        jml_vm_push(OBJ_VAL(
            jml_obj_cfunction_new(AS_STRING(jml_vm_peek(0)),
                current->function, module)
        ));

        jml_hashmap_set(&module->globals,
            AS_STRING(jml_vm_peek(1)),
            jml_vm_peek(0)
        );

        jml_vm_pop_two();
        ++current;
    }
#endif
    return true;
#else
    return false;
#endif
}


void
jml_module_finalize(jml_obj_module_t *module)
{
#ifdef JML_PLATFORM_NIX
    if (module == NULL
        && module->handle == NULL)
        return;

    jml_mfunction free_function = dlsym(
        module->handle, "module_free"
    );

    if (free_function != NULL)
        free_function(module);

    jml_module_close(module);
#endif
}
