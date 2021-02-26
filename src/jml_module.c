#include <stdio.h>
#include <stdlib.h>

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


#define SHARED_LOAD(a, b)           dlopen(a, b)
#define SHARED_FREE(a)              dlclose(a)
#define SHARED_SYM(a, b)            dlsym(a, b)

#elif defined JML_PLATFORM_WIN

#include <windows.h>

#define SHARED_LIB_EXT              "dll"

#define SHARED_LOAD(a)              LoadLibrary(a)
#define SHARED_FREE(a)              FreeLibrary(a)
#define SHARED_SYM(a, b)            GetProcAddress(a, b)

#endif


static int
jml_module_path(char *path)
{
    char *temp = getenv("JML_PATH");
    if (temp == NULL)
        temp = "std";

    return snprintf(path, JML_PATH_MAX, "%s", temp);
}


static bool
jml_module_resolve(jml_obj_string_t *qualified,
    char *path, size_t *path_len)
{
#define TRY_PATH(path_try, ext)                         \
    do {                                                \
        offset2 = snprintf(path_try, JML_PATH_MAX,      \
            "%s." ext, path);                           \
                                                        \
        if (offset2 < 0)                                \
            goto err;                                   \
        path_try[offset2] = '\0';                       \
                                                        \
        if (jml_file_exist(path_try)) {                 \
            offset += sprintf(path + offset, "." ext);  \
            path[offset] = '\0';                        \
            *path_len = offset;                         \
                                                        \
            jml_free(buffer);                           \
            return true;                                \
        }                                               \
    } while (false);


    char            *buffer     = jml_alloc(qualified->length + 1);
    memcpy(buffer, qualified->chars, qualified->length + 1);

    char            *save       = NULL;
    char            *token      = jml_strtok(buffer, ".", &save);

    char            *paths[64];
    uint8_t          last       = 0;

    for (; token != NULL; ++last) {
        if (last <= 64)
            paths[last] = token;
        else
            goto err;

        token = jml_strtok(NULL, ".", &save);
    }

    int32_t          offset     = jml_module_path(path);
    int32_t          offset2    = 0;
    char             path_dll[JML_PATH_MAX];
    char             path_jml[JML_PATH_MAX];

    for (int i = 0; i < last; ++i) {
        offset += snprintf(
            path + offset,
            JML_PATH_MAX,
            JML_PATH_SEPARATOR "%s",
            paths[i]
        );
    }

    path[offset] = '\0';

    TRY_PATH(path_dll, SHARED_LIB_EXT);
    TRY_PATH(path_jml, "jml");

    if (jml_file_isdir(path)) {
        offset += snprintf(
            path + offset,
            JML_PATH_MAX,
            JML_PATH_SEPARATOR "%s",
            vm->module_string->chars
        );

        path[offset] = '\0';
    }

    TRY_PATH(path_dll, SHARED_LIB_EXT);
    TRY_PATH(path_jml, "jml");

    goto err;

err:
    jml_free(buffer);
    *path       = '\0';
    *path_len   = 0;
    return false;


#undef TRY_PATH
}


jml_obj_module_t *
jml_module_open(jml_obj_string_t *qualified,
    jml_obj_string_t *name, jml_value_t *path)
{
    jml_obj_module_t *module;

    char    path_raw[JML_PATH_MAX];
    size_t  path_len = 0;

    if (!jml_module_resolve(qualified, path_raw, &path_len)
        || path_len == 0 || *path_raw == '\0'
        || !jml_file_exist(path_raw)) {

        jml_vm_error("ImportErr: Module not found.");
        return NULL;
    }

    if (jml_strsfx(path_raw, path_len,
        "." SHARED_LIB_EXT, strlen("." SHARED_LIB_EXT))) {

#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

        jml_module_handle_t handle = SHARED_LOAD(path_raw, RTLD_NOW);

        if (handle == NULL) {
            jml_vm_error("ImportErr: %s.", dlerror());
            goto err;
        }
#elif defined JML_PLATFORM_WIN

        jml_module_handle_t handle = SHARED_LOAD(path_raw);

        if (handle == NULL) {
            jml_vm_error("ImportErr: Loading error.");
            goto err;
        }
#else
        jml_vm_error(
            "ImportErr: Module loading not supported on %s.",
            JML_PLATFORM_STRING
        );
        goto err;
#endif
        module = jml_obj_module_new(name, handle);

    } else if (jml_strsfx(path_raw, path_len, ".jml", strlen(".jml"))) {

        module = jml_obj_module_new(name, NULL);
        jml_gc_exempt(OBJ_VAL(module));

        char *source = jml_file_read(path_raw);
        jml_obj_function_t *main = jml_compiler_compile(source, module, true);
        jml_free(source);

        if (main == NULL) {
            jml_gc_unexempt(OBJ_VAL(module));
            jml_vm_error("ImportErr: Import of '%s' failed.", name->chars);
            goto err;
        }

        jml_vm_push(OBJ_VAL(main));
        jml_obj_closure_t *closure = jml_obj_closure_new(main);
        jml_vm_pop();

        jml_obj_module_t *super = vm->current;
        vm->current = module;
        uint8_t frame_count = vm->frame_count;
        uint64_t offset     = vm->stack_top - vm->stack;
        vm->stack_top = vm->cstack;

        jml_vm_push(OBJ_VAL(closure));
        jml_vm_call_value(OBJ_VAL(closure), 0);

        if (jml_vm_run(NULL) != INTERPRET_OK) {
            jml_gc_unexempt(OBJ_VAL(module));
            vm->frame_count = frame_count;
            vm->current = super;
            vm->stack_top = vm->stack + offset;
            jml_vm_error("ImportErr: Import of '%s' failed.", name->chars);
            goto err;
        }

        vm->frame_count = frame_count;
        vm->current = super;
        vm->stack_top = vm->stack + offset;

    } else {
        jml_vm_error("ImportErr: Module not loadable.");
        goto err;
    }

    jml_vm_push(OBJ_VAL(module));
    jml_obj_string_t *path_str = jml_obj_string_copy(path_raw, path_len);
    jml_vm_push(OBJ_VAL(path_str));

    *path = jml_vm_peek(0);
    jml_vm_pop_two();
    return module;

err:
    *path = NONE_VAL;
    return NULL;
}


bool
jml_module_close(jml_obj_module_t *module)
{
    if (module == NULL || module->handle == NULL)
        return false;

    SHARED_FREE(module->handle);
    return true;
}


jml_obj_cfunction_t *
jml_module_get_raw(jml_obj_module_t *module,
    const char *function_name, bool silent)
{
    jml_cfunction raw = (jml_cfunction)SHARED_SYM(
        module->handle, function_name);

#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC
    char *result = dlerror();
    if (result || raw == NULL) {
        if (!silent)
            jml_vm_error("ImportErr: %s.", result);

        return NULL;
    }
#else
    if (raw == NULL) {
        if (!silent)
            jml_vm_error("ImportErr: Could not load dll.");

        return NULL;
    }
#endif

    jml_vm_push(jml_string_intern(function_name));

    jml_obj_cfunction_t *cfunction = jml_obj_cfunction_new(
        AS_STRING(jml_vm_peek(0)), raw, module);

    jml_vm_pop();

    return cfunction;
}


void
jml_module_register(jml_obj_module_t *module,
    jml_module_function *table)
{
    jml_module_function *current;
    if ((current = table) != NULL) {
        while (current->name != NULL
            && current->function != NULL) {

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
    }
}


bool
jml_module_initialize(jml_obj_module_t *module)
{
    if (module->handle == NULL)
        return true;

    jml_mfunction initializer = (jml_mfunction)SHARED_SYM(
        module->handle, "module_init"
    );

    if (initializer != NULL) {
        initializer(module);
    }

#ifndef JML_LAZY_IMPORT
    jml_module_function *table = (jml_module_function*)SHARED_SYM(
        module->handle, "module_table");

    jml_module_function *current;
    if ((current = table) != NULL) {
        while (current->name != NULL
            && current->function != NULL) {

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
    }
#endif
    return true;
}


void
jml_module_finalize(jml_obj_module_t *module)
{
    if (module == NULL
        && module->handle == NULL)
        return;

    jml_mfunction free_function = (jml_mfunction)SHARED_SYM(
        module->handle, "module_free"
    );

    if (free_function != NULL)
        free_function(module);

    jml_module_close(module);
}


bool
jml_module_add_value(jml_obj_module_t *module,
    const char *name, jml_value_t value)
{
    if (module == NULL || name == NULL)
        return false;

    jml_vm_push(value);
    jml_vm_push(jml_string_intern(name));

    jml_hashmap_set(&module->globals,
        AS_STRING(jml_vm_peek(0)), value);

    jml_vm_pop_two();
    return true;
}


bool
jml_module_add_class(jml_obj_module_t *module, const char *name,
    jml_module_function *table, bool inheritable)
{
    if (module == NULL || name == NULL)
        return false;

    jml_vm_push(jml_string_intern(name));

    jml_vm_push(OBJ_VAL(
        jml_obj_class_new(AS_STRING(jml_vm_peek(0)))
    ));

    jml_obj_class_t *klass = AS_CLASS(jml_vm_peek(0));

    klass->inheritable      = inheritable;
    klass->module           = module;

    jml_module_function *current;
    if ((current = table) != NULL) {
        while (current->name != NULL
            && current->function != NULL) {

            jml_vm_push(jml_string_intern(current->name));

            jml_obj_cfunction_t *method = jml_obj_cfunction_new(
                AS_STRING(jml_vm_peek(0)),
                current->function,
                module
            );

            jml_vm_push(OBJ_VAL(method));
            method->klass_name = klass->name;

            jml_hashmap_set(
                &klass->methods,
                AS_STRING(jml_vm_peek(1)),
                jml_vm_peek(0)
            );

            jml_vm_pop_two();
            ++current;
        }
    }

    jml_hashmap_set(&module->globals,
        AS_STRING(jml_vm_peek(1)), jml_vm_peek(0));

    jml_vm_pop_two();
    return true;
}
