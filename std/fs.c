#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <jml.h>


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#else

#error Current platform not supported.

#endif


static jml_obj_class_t *file_class      = NULL;


typedef struct {
    const char                     *name;
    DIR                            *handle;
    bool                            open;
} jml_std_fs_dir_t;


static jml_std_fs_dir_t *
jml_std_fs_dir_internal_init(const char *name, DIR *handle)
{
    jml_std_fs_dir_t *internal  = jml_alloc(sizeof(jml_std_fs_dir_t));

    internal->name              = name;
    internal->handle            = handle;
    internal->open              = true;

    return internal;
}


static jml_value_t
jml_std_fs_dir_init(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    jml_obj_instance_t  *self   = AS_INSTANCE(args[1]);
    const char          *name   = AS_CSTRING(args[0]);

    if (!jml_file_isdir(name)) {
        exc = jml_obj_exception_new(
            "DirErr",
            "Filename doesn't point to a directory."
        );
        goto err;
    }

    DIR *handle = opendir(name);

    if (handle == NULL) {
        exc = jml_obj_exception_new(
            "DirErr",
            "Opening Dir failed."
        );
        goto err;
    }

    jml_std_fs_dir_t *internal  = jml_std_fs_dir_internal_init(
        name, handle);

    self->extra                 = internal;

    jml_hashmap_set(&self->fields, jml_obj_string_copy("name", 4), args[0]);
    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_dir_open(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    jml_obj_instance_t  *self   = AS_INSTANCE(args[1]);
    const char          *name   = AS_CSTRING(args[0]);
    jml_std_fs_dir_t    *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Dir instance");
        goto err;
    }

    if (internal->open || internal->handle != NULL) {
        exc = jml_obj_exception_new(
            "DirErr",
            "Dir alredy opened."
        );
        goto err;
    }

    if (!jml_file_isdir(name)) {
        exc = jml_obj_exception_new(
            "DirErr",
            "Filename doesn't point to a directory."
        );
        goto err;
    }

    DIR *handle = opendir(name);

    if (handle == NULL) {
        exc = jml_obj_exception_new(
            "DirErr",
            "Opening Dir failed."
        );
        goto err;
    }

    internal->name              = name;
    internal->handle            = handle;
    internal->open              = true;

    jml_hashmap_set(&self->fields, jml_obj_string_copy("name", 4), args[1]);
    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_dir_close(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_fs_dir_t   *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Dir instance");
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "DirErr",
            "Dir alredy closed."
        );
        goto err;
    }

    if (closedir(internal->handle) == -1) {
        exc = jml_obj_exception_new(
            "DirErr",
            "Closing Dir failed."
        );
        goto err;
    }

    internal->name              = NULL;
    internal->handle            = NULL;
    internal->open              = false;

    jml_hashmap_set(&self->fields, jml_obj_string_copy("name", 4), NONE_VAL);
    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_dir_read(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_fs_dir_t   *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Dir instance");
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "DirErr",
            "Dir instance is closed."
        );
        goto err;
    }

    jml_obj_array_t *entries = jml_obj_array_new();
    jml_value_t      value   = OBJ_VAL(entries);
    jml_gc_exempt_push(value);

    struct dirent *entry;
    while ((entry = readdir(internal->handle)) != NULL) {
        jml_obj_array_append(
            entries, jml_string_intern(entry->d_name)
        );
    }

    rewinddir(internal->handle);

    jml_gc_exempt_pop();
    return value;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_dir_free(int arg_count, jml_value_t *args)
{
    jml_obj_instance_t *self = AS_INSTANCE(args[arg_count - 1]);

    if (self->extra != NULL) {
        jml_std_fs_dir_close(1, &args[arg_count - 1]);
        jml_free(self->extra);
        self->extra = NULL;
    }

    return NONE_VAL;
}


/*class table*/
MODULE_TABLE_HEAD dir_table[] = {
    {"__init",                      &jml_std_fs_dir_init},
    {"open",                        &jml_std_fs_dir_open},
    {"close",                       &jml_std_fs_dir_close},
    {"read",                        &jml_std_fs_dir_read},
    {"__free",                      &jml_std_fs_dir_free},
    {NULL,                          NULL}
};


typedef enum {
    INVALID,
    READ,
    READ_BIN,
    WRITE,
    WRITE_BIN,
    APPEND,
    APPEND_BIN,
    READ_WRITE,
    READ_WRITE_BIN,
    WRITE_READ,
    WRITE_READ_BIN,
    APPEND_READ,
    APPEND_READ_BIN
} jml_file_mode;


static jml_file_mode
jml_std_fs_file_open_mode(jml_obj_string_t *string)
{
    if (string->length < 1 || string->length > 3)
        return INVALID;

    switch (string->chars[0]) {
        case 'r':
            if (string->length > 1) {
                switch (string->chars[1]) {
                    case 'b':
                        if (string->length > 2)
                            return (string->chars[2] == '+') ? READ_WRITE_BIN : INVALID;
                        return READ_BIN;

                    case '+':
                        return READ_WRITE;
                }
                return INVALID;
            }
            return READ;

        case 'w':
            if (string->length > 1) {
                switch (string->chars[1]) {
                    case 'b':
                        if (string->length > 2)
                            return (string->chars[2] == '+') ? WRITE_READ_BIN : INVALID;
                        return WRITE_BIN;

                    case '+':
                        return WRITE_READ;
                }
                return INVALID;
            }
            return WRITE;

        case 'a':
            if (string->length > 1) {
                switch (string->chars[1]) {
                    case 'b':
                        if (string->length > 2)
                            return (string->chars[2] == '+') ? APPEND_READ_BIN : INVALID;
                        return APPEND_BIN;

                    case '+':
                        return APPEND_READ;
                }
                return INVALID;
            }
            return APPEND;
    }

    return INVALID;
}


typedef struct {
    const char                     *name;
    FILE                           *handle;
    jml_file_mode                   mode;
    bool                            open;
} jml_std_fs_file_t;


static jml_std_fs_file_t *
jml_std_fs_file_internal_init(const char *name,
    FILE *handle, jml_file_mode mode)
{
    jml_std_fs_file_t *internal = jml_alloc(sizeof(jml_std_fs_file_t));

    internal->name              = name;
    internal->handle            = handle;
    internal->mode              = mode;
    internal->open              = true;

    return internal;
}


static jml_value_t
jml_std_fs_file_init(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        exc = jml_error_types(false, 2, "string", "string");
        goto err;
    }

    jml_obj_instance_t  *self   = AS_INSTANCE(args[2]);
    const char          *name   = AS_CSTRING(args[0]);
    jml_obj_string_t    *mode   = AS_STRING(args[1]);
    jml_file_mode       open_mode;

    if ((open_mode = jml_std_fs_file_open_mode(mode)) == INVALID) {
        exc = jml_error_value("File open mode");
        goto err;
    }

    if (jml_file_isdir(name)) {
        exc = jml_obj_exception_new(
            "FileErr",
            "Filename points to a directory."
        );
        goto err;
    }

    FILE *handle = fopen(name, mode->chars);

    if (handle == NULL) {
        exc = jml_obj_exception_new(
            "FileErr",
            "Opening File failed."
        );
        goto err;
    }

    jml_std_fs_file_t *internal = jml_std_fs_file_internal_init(
        name, handle, open_mode);

    self->extra                 = internal;
    fseek(internal->handle, 0, SEEK_SET);

    jml_hashmap_set(&self->fields, jml_obj_string_copy("mode", 4), args[1]);
    jml_hashmap_set(&self->fields, jml_obj_string_copy("name", 4), args[0]);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_file_open(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        exc = jml_error_types(false, 2, "string", "string");
        goto err;
    }

    jml_obj_instance_t *self    = AS_INSTANCE(args[2]);
    const char         *name    = AS_CSTRING(args[0]);
    jml_obj_string_t   *mode    = AS_STRING(args[1]);
    jml_std_fs_file_t  *internal;
    jml_file_mode       open_mode;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("File instance");
        goto err;
    }

    if (internal->open || internal->handle != NULL) {
        exc = jml_obj_exception_new(
            "FileErr",
            "File alredy opened."
        );
        goto err;
    }

    if ((open_mode = jml_std_fs_file_open_mode(mode)) == INVALID) {
        exc = jml_error_value("File open mode");
        goto err;
    }

    if (jml_file_isdir(name)) {
        exc = jml_obj_exception_new(
            "FileErr",
            "Filename points to a directory."
        );
        goto err;
    }

    FILE *handle = fopen(name, mode->chars);

    if (handle == NULL) {
        exc = jml_obj_exception_new(
            "FileErr",
            "Opening File failed."
        );
        goto err;
    }

    internal->name              = name;
    internal->handle            = handle;
    internal->mode              = open_mode;
    internal->open              = true;

    fseek(internal->handle, 0, SEEK_SET);

    jml_hashmap_set(&self->fields, jml_obj_string_copy("mode", 4), args[1]);
    jml_hashmap_set(&self->fields, jml_obj_string_copy("name", 4), args[0]);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_file_close(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_fs_file_t  *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("File instance");
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "FileErr",
            "File alredy closed."
        );
        goto err;
    }

    if (fclose(internal->handle) == -1) {
        exc = jml_obj_exception_new(
            "FileErr",
            "Closing File failed."
        );
        goto err;
    }

    internal->name              = NULL;
    internal->handle            = NULL;
    internal->mode              = INVALID;
    internal->open              = false;

    jml_hashmap_set(&self->fields, jml_obj_string_copy("mode", 4), NONE_VAL);
    jml_hashmap_set(&self->fields, jml_obj_string_copy("name", 4), NONE_VAL);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_file_read(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_fs_file_t  *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("File instance");
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "FileErr",
            "File instance is closed."
        );
        goto err;
    }

    switch (internal->mode) {
        case READ:
        case READ_WRITE:
        case WRITE_READ:
        case APPEND_READ:
        case READ_BIN:
        case READ_WRITE_BIN:
        case WRITE_READ_BIN:
        case APPEND_READ_BIN: {
            fseek(internal->handle, 0, SEEK_END);
            size_t size     = ftell(internal->handle);
            rewind(internal->handle);

            char *buffer    = jml_realloc(NULL, size + 1);
            size_t bytes    = fread(buffer, sizeof(char),
                size, internal->handle);

            if (bytes < size) {
                jml_free(buffer);
                exc = jml_obj_exception_new(
                    "FileErr",
                    "Reading File failed."
                );
                goto err;
            }

            buffer[bytes] = '\0';

            return OBJ_VAL(jml_obj_string_take(
                buffer, bytes));
        }

        default: {
            exc = jml_obj_exception_new(
                "FileErr",
                "File without read permission."
            );
            goto err;
        }
    }

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_file_write(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    jml_obj_instance_t *self = AS_INSTANCE(args[1]);
    jml_obj_string_t   *string = AS_STRING(args[0]);
    jml_std_fs_file_t  *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("File instance");
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "FileErr",
            "File instance is closed."
        );
        goto err;
    }

    switch (internal->mode) {
        case READ_WRITE:
        case WRITE:
        case WRITE_READ:
        case APPEND:
        case APPEND_READ:
        case READ_WRITE_BIN:
        case WRITE_BIN:
        case WRITE_READ_BIN:
        case APPEND_BIN:
        case APPEND_READ_BIN: {
            size_t bytes = fwrite(string->chars,
                string->length, 1, internal->handle);

            if (bytes != 1) {
                exc = jml_obj_exception_new(
                    "FileErr",
                    "Writing to File failed."
                );
                goto err;
            }

            return NONE_VAL;
        }

        default: {
            exc = jml_obj_exception_new(
                "FileErr",
                "File without write or append permission."
            );
            goto err;
        }
    }

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_file_flush(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_fs_file_t  *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("File instance");
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "FileErr",
            "File instance is closed."
        );
        goto err;
    }

    if (fflush(internal->handle) == -1) {
        exc = jml_obj_exception_new(
            "FileErr",
            "Flushing File failed."
        );
        goto err;
    }

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_file_free(int arg_count, jml_value_t *args)
{
    jml_obj_instance_t *self = AS_INSTANCE(args[arg_count - 1]);

    if (self->extra != NULL) {
        jml_std_fs_dir_close(1, &args[arg_count - 1]);
        jml_free(self->extra);
        self->extra = NULL;
    }

    return NONE_VAL;
}


/*class table*/
MODULE_TABLE_HEAD file_table[] = {
    {"__init",                      &jml_std_fs_file_init},
    {"open",                        &jml_std_fs_file_open},
    {"close",                       &jml_std_fs_file_close},
    {"read",                        &jml_std_fs_file_read},
    {"write",                       &jml_std_fs_file_write},
    {"flush",                       &jml_std_fs_file_flush},
    {"__free",                      &jml_std_fs_file_free},
    {NULL,                          NULL}
};


static jml_value_t
jml_std_fs_remove(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    if (jml_file_exist(AS_CSTRING(args[0]))) {
        if (remove(AS_CSTRING(args[0])) != 0) {
            exc = jml_obj_exception_new(
                "FileErr",
                "Removing File failed."
            );
            goto err;
        }
        return NONE_VAL;

    } else {
        exc = jml_obj_exception_new(
            "FileErr",
            "File doesn't exist."
        );
        goto err;
    }

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_rename(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        exc = jml_error_types(false, 2, "string", "string");
        goto err;
    }

    if (jml_file_exist(AS_CSTRING(args[0]))) {
        if (rename(AS_CSTRING(args[0]), AS_CSTRING(args[1])) != 0) {
            exc = jml_obj_exception_new(
                "FileErr",
                "Renaming File failed."
            );
            goto err;
        }
        return NONE_VAL;

    } else {
        exc = jml_obj_exception_new(
            "FileErr",
            "File doesn't exist."
        );
        goto err;
    }

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_tempfile(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 0);

    if (exc != NULL)
        goto err;

    if (file_class == NULL) {
        exc = jml_error_value("File class");
        goto err;
    }

    FILE *handle = tmpfile();

    if (handle == NULL) {
        exc = jml_obj_exception_new(
            "FileErr",
            "Opening File failed."
        );
        goto err;
    }

    jml_obj_instance_t *file = jml_obj_instance_new(file_class);
    jml_std_fs_file_t *internal = jml_std_fs_file_internal_init(
        "", handle, WRITE_READ_BIN);

    file->extra = internal;

    jml_hashmap_set(&file->fields, jml_obj_string_copy("mode", 4), jml_string_intern("wb+"));
    jml_hashmap_set(&file->fields, jml_obj_string_copy("name", 4), NONE_VAL);

    return OBJ_VAL(file);

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_tempname(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 0);

    if (exc != NULL)
        goto err;

    char name[L_tmpnam];
    if (tmpnam(name) == NULL) {
        exc = jml_obj_exception_new(
            "SystemErr",
            "Call to 'tmpnam' failed."
        );
        goto err;
    }

    return jml_string_intern(name);

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_makedir(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    const char *name = AS_CSTRING(args[0]);

    if (mkdir(name, 0777) == - 1) {
        exc = jml_obj_exception_new(
            "SystemErr",
            "Call to 'mkdir' failed."
        );
        goto err;
    }

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"remove",                      &jml_std_fs_remove},
    {"rename",                      &jml_std_fs_rename},
    {"tempfile",                    &jml_std_fs_tempfile},
    {"tempname",                    &jml_std_fs_tempname},
    {"makedir",                     &jml_std_fs_makedir},
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
    jml_module_add_value(module, "FOPEN_MAX", NUM_VAL(FOPEN_MAX));
    jml_module_add_value(module, "FILENAME_MAX", NUM_VAL(FILENAME_MAX));
    jml_module_add_value(module, "TMP_MAX", NUM_VAL(TMP_MAX));

    jml_module_add_class(module, "File", file_table, false);
    jml_module_add_class(module, "Dir", dir_table, false);

    jml_value_t *file_value;
    if (jml_hashmap_get(&module->globals,
        jml_obj_string_copy("File", 4), &file_value)) {

        file_class = AS_CLASS(*file_value);
        /*jml_gc_exempt(*file_value);*/
    }
}
