//--link: uuid

#include <jml.h>


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#include <uuid/uuid.h>

#elif defined JML_PLATFORM_WIN

#pragma comment(lib, "rpcrt4.lib")
#include <windows.h>
#include <rpcdce.h>

#else

#error "Current platform not supported."

#endif


static jml_value_t
jml_std_uuid_uuid(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    const size_t length = 36;
    char *buffer = jml_realloc(NULL, length + 1);

#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC
    uuid_t uuid;
    uuid_generate_random(uuid);

    buffer = jml_realloc(NULL, length + 1);
    uuid_unparse(uuid, buffer);

#elif defined JML_PLATFORM_WIN
    UUID uuid;
    char *temp;

    RPC_STATUS status = UuidCreate(&uuid);
    switch (status) {
        case RPC_S_OK:
        case RPC_S_UUID_LOCAL_ONLY:
            UuidToStringA(&uuid, &temp);
            memcpy(buffer, temp, length);
            RpcStringFreeA(&temp);
            break;

        default:
            exc = jml_obj_exception_format(
                "WinApiErr",
                "Uuid creation failed, error code %d.",
                status
            );
            return OBJ_VAL(exc);
    }
#endif

    return OBJ_VAL(
        jml_obj_string_take(buffer, length)
    );
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"uuid",                        &jml_std_uuid_uuid},
    {NULL,                          NULL}
};
