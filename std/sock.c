#ifdef __GNUC__

#define _POSIX_C_SOURCE             200112l

#endif

#include <stdio.h>
#include <stdlib.h>

#include <jml.h>


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/in.h>

#include <arpa/inet.h>
#include <arpa/telnet.h>

#include <unistd.h>
#include <netdb.h>

#else

#error "Current platform not supported."

#endif


static jml_obj_class_t  *socket_class   = NULL;


typedef struct {
    int                             fd;
    int                             domain;
    int                             type;
    bool                            open;
    bool                            bound;
    bool                            connd;
} jml_std_sock_socket_t;


static jml_std_sock_socket_t *
jml_std_sock_socket_internal_init(int fd, int domain, int type)
{
    jml_std_sock_socket_t *internal = jml_alloc(sizeof(jml_std_sock_socket_t));

    internal->fd                = fd;
    internal->domain            = domain;
    internal->type              = type;
    internal->open              = true;
    internal->bound             = false;
    internal->connd             = false;

    return internal;
}


static jml_value_t
jml_std_sock_socket_init(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0]) || !IS_NUM(args[1])) {
        return OBJ_VAL(jml_error_types(
            false, 2, "number", "number"
        ));
    }

    jml_obj_instance_t  *self   = AS_INSTANCE(args[2]);
    int                  domain = AS_NUM(args[0]);
    int                  type   = AS_NUM(args[1]);
    int                  fd     = -1;

    if ((fd = socket(domain, type, 0)) < 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    jml_std_sock_socket_t *internal = jml_std_sock_socket_internal_init(
        fd, domain, type);

    self->extra = internal;

    jml_hashmap_set(&self->fields, jml_obj_string_copy("domain", 6), args[0]);
    jml_hashmap_set(&self->fields, jml_obj_string_copy("type", 4), args[1]);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_open(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0]) || !IS_NUM(args[1])) {
        return OBJ_VAL(jml_error_types(
            false, 2, "number", "number"
        ));
    }

    jml_obj_instance_t  *self   = AS_INSTANCE(args[2]);
    int                  domain = AS_NUM(args[0]);
    int                  type   = AS_NUM(args[1]);
    int                  fd     = -1;

    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (internal->open || internal->fd != -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket alredy opened."
        );
        goto err;
    }

    if ((fd = socket(domain, type, 0)) < 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    internal->fd                = fd;
    internal->domain            = domain;
    internal->type              = type;
    internal->open              = true;
    internal->bound             = false;
    internal->connd             = false;

    jml_hashmap_set(&self->fields, jml_obj_string_copy("domain", 6), args[0]);
    jml_hashmap_set(&self->fields, jml_obj_string_copy("type", 4), args[1]);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_bind(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return OBJ_VAL(jml_error_types(
            false, 2, "string", "string"
        ));
    }

    jml_obj_instance_t *self    = AS_INSTANCE(args[2]);
    const char         *name    = AS_CSTRING(args[0]);
    const char         *port    = AS_CSTRING(args[1]);

    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (!internal->open || internal->fd == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket is closed."
        );
        goto err;
    }

    if (internal->bound) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket alredy bound."
        );
        goto err;
    }

    struct addrinfo hint, *result;
    int status;

    memset(&hint, 0, sizeof(struct addrinfo));

    hint.ai_family   = internal->domain;
    hint.ai_socktype = internal->type;

    if ((status = getaddrinfo(name, port, &hint, &result)) != 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%s", gai_strerror(status)
        );
        goto err;
    }

    status = bind(internal->fd, result->ai_addr, result->ai_addrlen);

    if (status < 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    freeaddrinfo(result);
    internal->bound = true;

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_listen(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self    = AS_INSTANCE(args[0]);
    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (!internal->open || internal->fd == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket is closed."
        );
        goto err;
    }

    if (internal->bound) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket not bound."
        );
        goto err;
    }

    if (listen(internal->fd, 10) < 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_accept(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self    = AS_INSTANCE(args[0]);
    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (!internal->open || internal->fd == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket is closed."
        );
        goto err;
    }

    if (internal->bound) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket not bound."
        );
        goto err;
    }

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(struct sockaddr_storage);

    int new_fd = accept(internal->fd, (struct sockaddr*)&addr, &addrlen);

    if (new_fd < 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    if (socket_class == NULL) {
        exc = jml_error_value("Socket class");
        goto err;
    }

    jml_obj_instance_t *new_socket = jml_obj_instance_new(
        socket_class);

    jml_std_sock_socket_t *new_internal = jml_std_sock_socket_internal_init(
        new_fd, internal->domain, internal->type);

    new_socket->extra = new_internal;

    jml_hashmap_set(&self->fields, jml_obj_string_copy("domain", 6), NUM_VAL(internal->domain));
    jml_hashmap_set(&self->fields, jml_obj_string_copy("type", 4), NUM_VAL(internal->type));

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_connect(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc    = jml_error_args(
        arg_count - 1, 2);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return OBJ_VAL(jml_error_types(
            false, 2, "string", "string"
        ));
    }

    jml_obj_instance_t *self    = AS_INSTANCE(args[2]);
    const char         *name    = AS_CSTRING(args[0]);
    const char         *port    = AS_CSTRING(args[1]);

    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (!internal->open || internal->fd == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket is closed."
        );
        goto err;
    }

    if (internal->bound) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket is bound."
        );
        goto err;
    }

    if (internal->connd) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket alredy connected."
        );
        goto err;
    }

    struct addrinfo hint, *result;
    int status;

    memset(&hint, 0, sizeof(struct addrinfo));

    hint.ai_family   = internal->domain;
    hint.ai_socktype = internal->type;

    if ((status = getaddrinfo(name, port, &hint, &result)) != 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%s", gai_strerror(status)
        );
        goto err;
    }

    status = connect(internal->fd, result->ai_addr, result->ai_addrlen);

    if (status < 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    freeaddrinfo(result);
    internal->connd = true;

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_recv(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 1);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0])) {
        return OBJ_VAL(jml_error_types(
            false, 1, "number"
        ));
    }

    jml_obj_instance_t *self = AS_INSTANCE(args[1]);
    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (!internal->open || internal->fd == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket instance is closed."
        );
        goto err;
    }

    if (AS_NUM(args[0]) < 0) {
        exc = jml_error_value("buffer size");
        goto err;
    }

    size_t size = AS_NUM(args[0]);
    char *buffer = jml_alloc(size + 1);

    ssize_t recvd = recv(internal->fd, buffer, size, 0);

    if (recvd < 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    return OBJ_VAL(
        jml_obj_string_take(buffer, recvd)
    );

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_send(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        return OBJ_VAL(jml_error_types(
            false, 1, "string"
        ));
    }

    jml_obj_instance_t *self = AS_INSTANCE(args[1]);
    jml_obj_string_t *string = AS_STRING(args[0]);
    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (!internal->open || internal->fd == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket instance is closed."
        );
        goto err;
    }

    ssize_t sent = send(
        internal->fd, string->chars, string->length + 1, 0
    );

    if (sent < 0) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    return NUM_VAL(sent);

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_setopt(int arg_count, jml_value_t *args)
{
    (void) arg_count;
    (void) args;

    return OBJ_VAL(
        jml_obj_exception_new("NotImplemented", "Not implemented")
    );
}


static jml_value_t
jml_std_sock_socket_shutdown(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (!internal->open || internal->fd == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket is closed."
        );
        goto err;
    }

    if (shutdown(internal->fd, SHUT_RDWR) == -1) {
        exc = jml_obj_exception_format(
            "SocketErr", "%m"
        );
        goto err;
    }

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_close(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count - 1, 0);

    if (exc != NULL)
        goto err;

    jml_obj_instance_t *self = AS_INSTANCE(args[0]);
    jml_std_sock_socket_t *internal;

    if ((internal = self->extra) == NULL) {
        exc = jml_error_value("Socket instance");
        goto err;
    }

    if (!internal->open || internal->fd == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Socket alredy closed."
        );
        goto err;
    }

    if (close(internal->fd) == -1) {
        exc = jml_obj_exception_new(
            "SocketErr",
            "Closing Socket failed."
        );
        goto err;
    }

    internal->domain            = -1;
    internal->type              = -1;
    internal->open              = false;
    internal->bound             = false;
    internal->connd             = false;

    jml_hashmap_set(&self->fields, jml_obj_string_copy("domain", 6), NONE_VAL);
    jml_hashmap_set(&self->fields, jml_obj_string_copy("type", 4), NONE_VAL);

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_sock_socket_free(int arg_count, jml_value_t *args)
{
    jml_obj_instance_t *self = AS_INSTANCE(args[arg_count - 1]);

    if (self->extra != NULL) {
        jml_std_sock_socket_close(1, &args[arg_count - 1]);
        jml_free(self->extra);
        self->extra = NULL;
    }

    return NONE_VAL;
}


/*class table*/
MODULE_TABLE_HEAD socket_table[] = {
    {"__init",                      &jml_std_sock_socket_init},
    {"open",                        &jml_std_sock_socket_open},
    {"bind",                        &jml_std_sock_socket_bind},
    {"listen",                      &jml_std_sock_socket_listen},
    {"accept",                      &jml_std_sock_socket_accept},
    {"connect",                     &jml_std_sock_socket_connect},
    {"recv",                        &jml_std_sock_socket_recv},
    {"send",                        &jml_std_sock_socket_send},
    {"setopt",                      &jml_std_sock_socket_setopt},
    {"shutdown",                    &jml_std_sock_socket_shutdown},
    {"close",                       &jml_std_sock_socket_close},
    {"__free",                      &jml_std_sock_socket_free},
    {NULL,                          NULL}
};


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
    jml_module_add_value(module, "AF_LOCAL", NUM_VAL(AF_LOCAL));
    jml_module_add_value(module, "AF_INET", NUM_VAL(AF_INET));
    jml_module_add_value(module, "AF_INET6", NUM_VAL(AF_INET6));
    jml_module_add_value(module, "AF_UNSPEC", NUM_VAL(AF_UNSPEC));

    jml_module_add_value(module, "SOCK_STREAM", NUM_VAL(SOCK_STREAM));
    jml_module_add_value(module, "SOCK_DGRAM", NUM_VAL(SOCK_DGRAM));
    jml_module_add_value(module, "SOCK_RAW", NUM_VAL(SOCK_RAW));
    jml_module_add_value(module, "SOCK_RDM", NUM_VAL(SOCK_RDM));
    jml_module_add_value(module, "SOCK_SEQPACKET", NUM_VAL(SOCK_SEQPACKET));

    jml_module_add_value(module, "SOMAXCONN", NUM_VAL(SOMAXCONN));

    jml_module_add_value(module, "SO_DEBUG", NUM_VAL(SO_DEBUG));
    jml_module_add_value(module, "SO_BROADCAST", NUM_VAL(SO_BROADCAST));
    jml_module_add_value(module, "SO_REUSEADDR", NUM_VAL(SO_REUSEADDR));
    jml_module_add_value(module, "SO_KEEPALIVE", NUM_VAL(SO_KEEPALIVE));
    jml_module_add_value(module, "SO_LINGER", NUM_VAL(SO_LINGER));
    jml_module_add_value(module, "SO_OOBINLINE", NUM_VAL(SO_OOBINLINE));
    jml_module_add_value(module, "SO_SNDBUF", NUM_VAL(SO_SNDBUF));
    jml_module_add_value(module, "SO_RCVBUF", NUM_VAL(SO_RCVBUF));
    jml_module_add_value(module, "SO_DONTROUTE", NUM_VAL(SO_DONTROUTE));
    jml_module_add_value(module, "SO_RCVLOWAT", NUM_VAL(SO_RCVLOWAT));
    jml_module_add_value(module, "SO_RCVTIMEO", NUM_VAL(SO_RCVTIMEO));
    jml_module_add_value(module, "SO_SNDLOWAT", NUM_VAL(SO_SNDLOWAT));
    jml_module_add_value(module, "SO_SNDTIMEO", NUM_VAL(SO_SNDTIMEO));

    jml_module_add_class(module, "Socket", socket_table, false);

    jml_value_t *socket_value;
    if (jml_hashmap_get(&module->globals,
        jml_obj_string_copy("Socket", 6), &socket_value)) {

        socket_class = AS_CLASS(*socket_value);
        /*jml_gc_exempt(*socket_value);*/
    }
}
