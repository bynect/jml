#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <jml.h>


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#endif


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


static jml_value_t
jml_std_fs_file_init(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_core_exception_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[1]) || !IS_STRING(args[2])) {
        exc = jml_core_exception_types(false, 2, "string");
        goto err;
    }

    jml_obj_instance_t  *self   = AS_INSTANCE(args[0]);
    const char          *name   = AS_CSTRING(args[1]);
    jml_obj_string_t    *mode   = AS_STRING(args[2]);
    jml_file_mode open_mode;

    if ((open_mode = jml_std_fs_file_open_mode(mode)) == INVALID) {
        exc = jml_core_exception_value("File open mode.");
        goto err;
    }

    if (jml_file_isdir(name)) {
        exc = jml_obj_exception_new(
            "IOError",
            "Filename points to a directory."
        );
        goto err;
    }

    FILE *handle = fopen(name, mode->chars);

    if (handle == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
            "Opening File failed."
        );
        goto err;
    }

    jml_std_fs_file_t *internal = jml_alloc(sizeof(jml_std_fs_file_t));

    internal->name              = name;
    internal->handle            = handle;
    internal->mode              = open_mode;
    internal->open              = true;

    self->extra                 = internal;
    fseek(internal->handle, 0, SEEK_SET);

    jml_obj_string_t *mode_string = jml_obj_string_copy("mode", 4);
    jml_obj_string_t *name_string = jml_obj_string_copy("name", 4);

    jml_gc_exempt_push(OBJ_VAL(mode_string));
    jml_gc_exempt_push(OBJ_VAL(name_string));

    jml_hashmap_set(&self->fields, mode_string, args[2]);
    jml_hashmap_set(&self->fields, name_string, args[1]);

    jml_gc_exempt_pop();
    jml_gc_exempt_pop();

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_file_close(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_fs_file_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
            "Invalid File instance."
        );
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
            "File alredy closed."
        );
        goto err;
    }

    if (fclose(internal->handle) == EOF) {
        exc = jml_obj_exception_new(
            "IOError",
            "Closing File failed."
        );
        goto err;
    }

    internal->name              = NULL;
    internal->handle            = NULL;
    internal->mode              = INVALID;
    internal->open              = false;

    jml_obj_string_t *mode_string = jml_obj_string_copy("mode", 4);
    jml_obj_string_t *name_string = jml_obj_string_copy("name", 4);

    jml_gc_exempt_push(OBJ_VAL(mode_string));
    jml_gc_exempt_push(OBJ_VAL(name_string));

    jml_hashmap_set(&self->fields, mode_string, NONE_VAL);
    jml_hashmap_set(&self->fields, name_string, NONE_VAL);

    jml_gc_exempt_pop();
    jml_gc_exempt_pop();

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_fs_file_read(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_fs_file_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
            "Invalid File instance."
        );
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
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
                    "IOError",
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
                "IOError",
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
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count - 1, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_core_exception_types(
            false, 1, "string"
        );
        goto err;
    }

    jml_obj_instance_t *self = AS_INSTANCE(args[1]);
    jml_obj_string_t *string = AS_STRING(args[0]);
    jml_std_fs_file_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
            "Invalid File instance."
        );
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
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
                    "IOError",
                    "Writing to File failed."
                );
                goto err;
            }

            return NONE_VAL;
        }

        default: {
            exc = jml_obj_exception_new(
                "IOError",
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
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_fs_file_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
            "Invalid File instance."
        );
        goto err;
    }

    if (!internal->open || internal->handle == NULL) {
        exc = jml_obj_exception_new(
            "IOError",
            "File instance is closed."
        );
        goto err;
    }

    if (fflush(internal->handle) == EOF) {
        exc = jml_obj_exception_new(
            "IOError",
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
        jml_std_fs_file_close(arg_count, args);
        jml_free(self->extra);
        self->extra = NULL;
    }

    return NONE_VAL;
}


/*class table*/
MODULE_TABLE_HEAD file_table[] = {
    {"__init",                      &jml_std_fs_file_init},
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
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_core_exception_types(
            false, 1, "string");
        goto err;
    }

    if (jml_file_exist(AS_CSTRING(args[0]))) {
        if (remove(AS_CSTRING(args[0])) != 0) {
            exc = jml_obj_exception_new(
                "IOError",
                "Removing File failed."
            );
            goto err;
        }
        return NONE_VAL;

    } else {
        exc = jml_obj_exception_new(
            "IOError",
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
    jml_obj_exception_t *exc = jml_core_exception_args(
        arg_count, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        exc = jml_core_exception_types(
            false, 2, "string");
        goto err;
    }

    if (jml_file_exist(AS_CSTRING(args[0]))) {
        if (rename(AS_CSTRING(args[0]), AS_CSTRING(args[1])) != 0) {
            exc = jml_obj_exception_new(
                "IOError",
                "Renaming File failed."
            );
            goto err;
        }
        return NONE_VAL;

    } else {
        exc = jml_obj_exception_new(
            "IOError",
            "File doesn't exist."
        );
        goto err;
    }

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"remove",                      &jml_std_fs_remove},
    {"rename",                      &jml_std_fs_rename},
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
    jml_module_add_value(module, "FOPEN_MAX", NUM_VAL(FOPEN_MAX));
    jml_module_add_value(module, "FILENAME_MAX", NUM_VAL(FILENAME_MAX));

    jml_module_add_class(module, "File", file_table, false);
}
