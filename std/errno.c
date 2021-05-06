#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <jml.h>

#if defined JML_PLATFORM_WIN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#undef EADDRINUSE
#undef EADDRNOTAVAIL
#undef EAFNOSUPPORT
#undef EALREADY
#undef ECONNABORTED
#undef ECONNREFUSED
#undef ECONNRESET
#undef EDESTADDRREQ
#undef EHOSTUNREACH
#undef EINPROGRESS
#undef EISCONN
#undef ELOOP
#undef EMSGSIZE
#undef ENETDOWN
#undef ENETRESET
#undef ENETUNREACH
#undef ENOBUFS
#undef ENOPROTOOPT
#undef ENOTCONN
#undef ENOTSOCK
#undef EOPNOTSUPP
#undef EPROTONOSUPPORT
#undef EPROTOTYPE
#undef ETIMEDOUT
#undef EWOULDBLOCK

#endif


static jml_value_t
jml_std_errno_errno(int arg_count, JML_UNUSED(jml_value_t *args))
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 0);

    if (exc != NULL)
        return OBJ_VAL(exc);

    return NUM_VAL(errno);
}


static jml_value_t
jml_std_errno_strerror(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0])) {
        jml_error_types(false, 1, "number");
        goto err;
    }

    char *str = strerror((int)AS_NUM(args[0]));
    return jml_string_intern(str);

err:
    return OBJ_VAL(exc);
}


static jml_value_t
jml_std_errno_perror(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 1);

    if (exc != NULL)
        goto err;

    if (!IS_STRING(args[0])) {
        exc = jml_error_types(false, 1, "string");
        goto err;
    }

    perror(AS_CSTRING(args[0]));
    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"errno",                       &jml_std_errno_errno},
    {"strerror",                    &jml_std_errno_strerror},
    {"perror",                      &jml_std_errno_perror},
    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
#ifdef ENODEV
    jml_module_add_value(module, "ENODEV", NUM_VAL(ENODEV));
#endif

#ifdef ENOCSI
    jml_module_add_value(module, "ENOCSI", NUM_VAL(ENOCSI));
#endif

#ifdef EHOSTUNREACH
    jml_module_add_value(module, "EHOSTUNREACH", NUM_VAL(EHOSTUNREACH));
#else
#ifdef WSAEHOSTUNREACH
    jml_module_add_value(module, "WSAEHOSTUNREACH", NUM_VAL(WSAEHOSTUNREACH));
#endif
#endif

#ifdef ENOMSG
    jml_module_add_value(module, "ENOMSG", NUM_VAL(ENOMSG));
#endif

#ifdef EUCLEAN
    jml_module_add_value(module, "EUCLEAN", NUM_VAL(EUCLEAN));
#endif

#ifdef EL2NSYNC
    jml_module_add_value(module, "EL2NSYNC", NUM_VAL(EL2NSYNC));
#endif

#ifdef EL2HLT
    jml_module_add_value(module, "EL2HLT", NUM_VAL(EL2HLT));
#endif

#ifdef ENODATA
    jml_module_add_value(module, "ENODATA", NUM_VAL(ENODATA));
#endif

#ifdef ENOTBLK
    jml_module_add_value(module, "ENOTBLK", NUM_VAL(ENOTBLK));
#endif

#ifdef ENOSYS
    jml_module_add_value(module, "ENOSYS", NUM_VAL(ENOSYS));
#endif

#ifdef EPIPE
    jml_module_add_value(module, "EPIPE", NUM_VAL(EPIPE));
#endif

#ifdef EINVAL
    jml_module_add_value(module, "EINVAL", NUM_VAL(EINVAL));
#else
#ifdef WSAEINVAL
    jml_module_add_value(module, "WSAEINVAL", NUM_VAL(WSAEINVAL));
#endif
#endif

#ifdef EOVERFLOW
    jml_module_add_value(module, "EOVERFLOW", NUM_VAL(EOVERFLOW));
#endif

#ifdef EADV
    jml_module_add_value(module, "EADV", NUM_VAL(EADV));
#endif

#ifdef EINTR
    jml_module_add_value(module, "EINTR", NUM_VAL(EINTR));
#else
#ifdef WSAEINTR
    jml_module_add_value(module, "WSAEINTR", NUM_VAL(WSAEINTR));
#endif
#endif

#ifdef EUSERS
    jml_module_add_value(module, "EUSERS", NUM_VAL(EUSERS));
#else
#ifdef WSAEUSERS
    jml_module_add_value(module, "WSAEUSERS", NUM_VAL(WSAEUSERS));
#endif
#endif

#ifdef ENOTEMPTY
    jml_module_add_value(module, "ENOTEMPTY", NUM_VAL(ENOTEMPTY));
#else
#ifdef WSAENOTEMPTY
    jml_module_add_value(module, "WSAENOTEMPTY", NUM_VAL(WSAENOTEMPTY));
#endif
#endif

#ifdef ENOBUFS
    jml_module_add_value(module, "ENOBUFS", NUM_VAL(ENOBUFS));
#else
#ifdef WSAENOBUFS
    jml_module_add_value(module, "WSAENOBUFS", NUM_VAL(WSAENOBUFS));
#endif
#endif

#ifdef EPROTO
    jml_module_add_value(module, "EPROTO", NUM_VAL(EPROTO));
#endif

#ifdef EREMOTE
    jml_module_add_value(module, "EREMOTE", NUM_VAL(EREMOTE));
#else
#ifdef WSAEREMOTE
    jml_module_add_value(module, "WSAEREMOTE", NUM_VAL(WSAEREMOTE));
#endif
#endif

#ifdef ENAVAIL
    jml_module_add_value(module, "ENAVAIL", NUM_VAL(ENAVAIL));
#endif

#ifdef ECHILD
    jml_module_add_value(module, "ECHILD", NUM_VAL(ECHILD));
#endif

#ifdef ELOOP
    jml_module_add_value(module, "ELOOP", NUM_VAL(ELOOP));
#else
#ifdef WSAELOOP
    jml_module_add_value(module, "WSAELOOP", NUM_VAL(WSAELOOP));
#endif
#endif

#ifdef EXDEV
    jml_module_add_value(module, "EXDEV", NUM_VAL(EXDEV));
#endif

#ifdef E2BIG
    jml_module_add_value(module, "E2BIG", NUM_VAL(E2BIG));
#endif

#ifdef ESRCH
    jml_module_add_value(module, "ESRCH", NUM_VAL(ESRCH));
#endif

#ifdef EMSGSIZE
    jml_module_add_value(module, "EMSGSIZE", NUM_VAL(EMSGSIZE));
#else
#ifdef WSAEMSGSIZE
    jml_module_add_value(module, "WSAEMSGSIZE", NUM_VAL(WSAEMSGSIZE));
#endif
#endif

#ifdef EAFNOSUPPORT
    jml_module_add_value(module, "EAFNOSUPPORT", NUM_VAL(EAFNOSUPPORT));
#else
#ifdef WSAEAFNOSUPPORT
    jml_module_add_value(module, "WSAEAFNOSUPPORT", NUM_VAL(WSAEAFNOSUPPORT));
#endif
#endif

#ifdef EBADR
    jml_module_add_value(module, "EBADR", NUM_VAL(EBADR));
#endif

#ifdef EHOSTDOWN
    jml_module_add_value(module, "EHOSTDOWN", NUM_VAL(EHOSTDOWN));
#else
#ifdef WSAEHOSTDOWN
    jml_module_add_value(module, "WSAEHOSTDOWN", NUM_VAL(WSAEHOSTDOWN));
#endif
#endif

#ifdef EPFNOSUPPORT
    jml_module_add_value(module, "EPFNOSUPPORT", NUM_VAL(EPFNOSUPPORT));
#else
#ifdef WSAEPFNOSUPPORT
    jml_module_add_value(module, "WSAEPFNOSUPPORT", NUM_VAL(WSAEPFNOSUPPORT));
#endif
#endif

#ifdef ENOPROTOOPT
    jml_module_add_value(module, "ENOPROTOOPT", NUM_VAL(ENOPROTOOPT));
#else
#ifdef WSAENOPROTOOPT
    jml_module_add_value(module, "WSAENOPROTOOPT", NUM_VAL(WSAENOPROTOOPT));
#endif
#endif

#ifdef EBUSY
    jml_module_add_value(module, "EBUSY", NUM_VAL(EBUSY));
#endif

#ifdef EWOULDBLOCK
    jml_module_add_value(module, "EWOULDBLOCK", NUM_VAL(EWOULDBLOCK));
#else
#ifdef WSAEWOULDBLOCK
    jml_module_add_value(module, "WSAEWOULDBLOCK", NUM_VAL(WSAEWOULDBLOCK));
#endif
#endif

#ifdef EBADFD
    jml_module_add_value(module, "EBADFD", NUM_VAL(EBADFD));
#endif

#ifdef EDOTDOT
    jml_module_add_value(module, "EDOTDOT", NUM_VAL(EDOTDOT));
#endif

#ifdef EISCONN
    jml_module_add_value(module, "EISCONN", NUM_VAL(EISCONN));
#else
#ifdef WSAEISCONN
    jml_module_add_value(module, "WSAEISCONN", NUM_VAL(WSAEISCONN));
#endif
#endif

#ifdef ENOANO
    jml_module_add_value(module, "ENOANO", NUM_VAL(ENOANO));
#endif

#ifdef ESHUTDOWN
    jml_module_add_value(module, "ESHUTDOWN", NUM_VAL(ESHUTDOWN));
#else
#ifdef WSAESHUTDOWN
    jml_module_add_value(module, "WSAESHUTDOWN", NUM_VAL(WSAESHUTDOWN));
#endif
#endif

#ifdef ECHRNG
    jml_module_add_value(module, "ECHRNG", NUM_VAL(ECHRNG));
#endif

#ifdef ELIBBAD
    jml_module_add_value(module, "ELIBBAD", NUM_VAL(ELIBBAD));
#endif

#ifdef ENONET
    jml_module_add_value(module, "ENONET", NUM_VAL(ENONET));
#endif

#ifdef EBADE
    jml_module_add_value(module, "EBADE", NUM_VAL(EBADE));
#endif

#ifdef EBADF
    jml_module_add_value(module, "EBADF", NUM_VAL(EBADF));
#else
#ifdef WSAEBADF
    jml_module_add_value(module, "WSAEBADF", NUM_VAL(WSAEBADF));
#endif
#endif

#ifdef EMULTIHOP
    jml_module_add_value(module, "EMULTIHOP", NUM_VAL(EMULTIHOP));
#endif

#ifdef EIO
    jml_module_add_value(module, "EIO", NUM_VAL(EIO));
#endif

#ifdef EUNATCH
    jml_module_add_value(module, "EUNATCH", NUM_VAL(EUNATCH));
#endif

#ifdef EPROTOTYPE
    jml_module_add_value(module, "EPROTOTYPE", NUM_VAL(EPROTOTYPE));
#else
#ifdef WSAEPROTOTYPE
    jml_module_add_value(module, "WSAEPROTOTYPE", NUM_VAL(WSAEPROTOTYPE));
#endif
#endif

#ifdef ENOSPC
    jml_module_add_value(module, "ENOSPC", NUM_VAL(ENOSPC));
#endif

#ifdef ENOEXEC
    jml_module_add_value(module, "ENOEXEC", NUM_VAL(ENOEXEC));
#endif

#ifdef EALREADY
    jml_module_add_value(module, "EALREADY", NUM_VAL(EALREADY));
#else
#ifdef WSAEALREADY
    jml_module_add_value(module, "WSAEALREADY", NUM_VAL(WSAEALREADY));
#endif
#endif

#ifdef ENETDOWN
    jml_module_add_value(module, "ENETDOWN", NUM_VAL(ENETDOWN));
#else
#ifdef WSAENETDOWN
    jml_module_add_value(module, "WSAENETDOWN", NUM_VAL(WSAENETDOWN));
#endif
#endif

#ifdef ENOTNAM
    jml_module_add_value(module, "ENOTNAM", NUM_VAL(ENOTNAM));
#endif

#ifdef EACCES
    jml_module_add_value(module, "EACCES", NUM_VAL(EACCES));
#else
#ifdef WSAEACCES
    jml_module_add_value(module, "WSAEACCES", NUM_VAL(WSAEACCES));
#endif
#endif

#ifdef ELNRNG
    jml_module_add_value(module, "ELNRNG", NUM_VAL(ELNRNG));
#endif

#ifdef EILSEQ
    jml_module_add_value(module, "EILSEQ", NUM_VAL(EILSEQ));
#endif

#ifdef ENOTDIR
    jml_module_add_value(module, "ENOTDIR", NUM_VAL(ENOTDIR));
#endif

#ifdef ENOTUNIQ
    jml_module_add_value(module, "ENOTUNIQ", NUM_VAL(ENOTUNIQ));
#endif

#ifdef EPERM
    jml_module_add_value(module, "EPERM", NUM_VAL(EPERM));
#endif

#ifdef EDOM
    jml_module_add_value(module, "EDOM", NUM_VAL(EDOM));
#endif

#ifdef EXFULL
    jml_module_add_value(module, "EXFULL", NUM_VAL(EXFULL));
#endif

#ifdef ECONNREFUSED
    jml_module_add_value(module, "ECONNREFUSED", NUM_VAL(ECONNREFUSED));
#else
#ifdef WSAECONNREFUSED
    jml_module_add_value(module, "WSAECONNREFUSED", NUM_VAL(WSAECONNREFUSED));
#endif
#endif

#ifdef EISDIR
    jml_module_add_value(module, "EISDIR", NUM_VAL(EISDIR));
#endif

#ifdef EPROTONOSUPPORT
    jml_module_add_value(module, "EPROTONOSUPPORT", NUM_VAL(EPROTONOSUPPORT));
#else
#ifdef WSAEPROTONOSUPPORT
    jml_module_add_value(module, "WSAEPROTONOSUPPORT", NUM_VAL(WSAEPROTONOSUPPORT));
#endif
#endif

#ifdef EROFS
    jml_module_add_value(module, "EROFS", NUM_VAL(EROFS));
#endif

#ifdef EADDRNOTAVAIL
    jml_module_add_value(module, "EADDRNOTAVAIL", NUM_VAL(EADDRNOTAVAIL));
#else
#ifdef WSAEADDRNOTAVAIL
    jml_module_add_value(module, "WSAEADDRNOTAVAIL", NUM_VAL(WSAEADDRNOTAVAIL));
#endif
#endif

#ifdef EIDRM
    jml_module_add_value(module, "EIDRM", NUM_VAL(EIDRM));
#endif

#ifdef ECOMM
    jml_module_add_value(module, "ECOMM", NUM_VAL(ECOMM));
#endif

#ifdef ESRMNT
    jml_module_add_value(module, "ESRMNT", NUM_VAL(ESRMNT));
#endif

#ifdef EREMOTEIO
    jml_module_add_value(module, "EREMOTEIO", NUM_VAL(EREMOTEIO));
#endif

#ifdef EL3RST
    jml_module_add_value(module, "EL3RST", NUM_VAL(EL3RST));
#endif

#ifdef EBADMSG
    jml_module_add_value(module, "EBADMSG", NUM_VAL(EBADMSG));
#endif

#ifdef ENFILE
    jml_module_add_value(module, "ENFILE", NUM_VAL(ENFILE));
#endif

#ifdef ELIBMAX
    jml_module_add_value(module, "ELIBMAX", NUM_VAL(ELIBMAX));
#endif

#ifdef ESPIPE
    jml_module_add_value(module, "ESPIPE", NUM_VAL(ESPIPE));
#endif

#ifdef ENOLINK
    jml_module_add_value(module, "ENOLINK", NUM_VAL(ENOLINK));
#endif

#ifdef ENETRESET
    jml_module_add_value(module, "ENETRESET", NUM_VAL(ENETRESET));
#else
#ifdef WSAENETRESET
    jml_module_add_value(module, "WSAENETRESET", NUM_VAL(WSAENETRESET));
#endif
#endif

#ifdef ETIMEDOUT
    jml_module_add_value(module, "ETIMEDOUT", NUM_VAL(ETIMEDOUT));
#else
#ifdef WSAETIMEDOUT
    jml_module_add_value(module, "WSAETIMEDOUT", NUM_VAL(WSAETIMEDOUT));
#endif
#endif

#ifdef ENOENT
    jml_module_add_value(module, "ENOENT", NUM_VAL(ENOENT));
#endif

#ifdef EEXIST
    jml_module_add_value(module, "EEXIST", NUM_VAL(EEXIST));
#endif

#ifdef EDQUOT
    jml_module_add_value(module, "EDQUOT", NUM_VAL(EDQUOT));
#else
#ifdef WSAEDQUOT
    jml_module_add_value(module, "WSAEDQUOT", NUM_VAL(WSAEDQUOT));
#endif
#endif

#ifdef ENOSTR
    jml_module_add_value(module, "ENOSTR", NUM_VAL(ENOSTR));
#endif

#ifdef EBADSLT
    jml_module_add_value(module, "EBADSLT", NUM_VAL(EBADSLT));
#endif

#ifdef EBADRQC
    jml_module_add_value(module, "EBADRQC", NUM_VAL(EBADRQC));
#endif

#ifdef ELIBACC
    jml_module_add_value(module, "ELIBACC", NUM_VAL(ELIBACC));
#endif

#ifdef EFAULT
    jml_module_add_value(module, "EFAULT", NUM_VAL(EFAULT));
#else
#ifdef WSAEFAULT
    jml_module_add_value(module, "WSAEFAULT", NUM_VAL(WSAEFAULT));
#endif
#endif

#ifdef EFBIG
    jml_module_add_value(module, "EFBIG", NUM_VAL(EFBIG));
#endif

#ifdef EDEADLK
    jml_module_add_value(module, "EDEADLK", NUM_VAL(EDEADLK));
#endif

#ifdef ENOTCONN
    jml_module_add_value(module, "ENOTCONN", NUM_VAL(ENOTCONN));
#else
#ifdef WSAENOTCONN
    jml_module_add_value(module, "WSAENOTCONN", NUM_VAL(WSAENOTCONN));
#endif
#endif

#ifdef EDESTADDRREQ
    jml_module_add_value(module, "EDESTADDRREQ", NUM_VAL(EDESTADDRREQ));
#else
#ifdef WSAEDESTADDRREQ
    jml_module_add_value(module, "WSAEDESTADDRREQ", NUM_VAL(WSAEDESTADDRREQ));
#endif
#endif

#ifdef ELIBSCN
    jml_module_add_value(module, "ELIBSCN", NUM_VAL(ELIBSCN));
#endif

#ifdef ENOLCK
    jml_module_add_value(module, "ENOLCK", NUM_VAL(ENOLCK));
#endif

#ifdef EISNAM
    jml_module_add_value(module, "EISNAM", NUM_VAL(EISNAM));
#endif

#ifdef ECONNABORTED
    jml_module_add_value(module, "ECONNABORTED", NUM_VAL(ECONNABORTED));
#else
#ifdef WSAECONNABORTED
    jml_module_add_value(module, "WSAECONNABORTED", NUM_VAL(WSAECONNABORTED));
#endif
#endif

#ifdef ENETUNREACH
    jml_module_add_value(module, "ENETUNREACH", NUM_VAL(ENETUNREACH));
#else
#ifdef WSAENETUNREACH
    jml_module_add_value(module, "WSAENETUNREACH", NUM_VAL(WSAENETUNREACH));
#endif
#endif

#ifdef ESTALE
    jml_module_add_value(module, "ESTALE", NUM_VAL(ESTALE));
#else
#ifdef WSAESTALE
    jml_module_add_value(module, "WSAESTALE", NUM_VAL(WSAESTALE));
#endif
#endif

#ifdef ENOSR
    jml_module_add_value(module, "ENOSR", NUM_VAL(ENOSR));
#endif

#ifdef ENOMEM
    jml_module_add_value(module, "ENOMEM", NUM_VAL(ENOMEM));
#endif

#ifdef ENOTSOCK
    jml_module_add_value(module, "ENOTSOCK", NUM_VAL(ENOTSOCK));
#else
#ifdef WSAENOTSOCK
    jml_module_add_value(module, "WSAENOTSOCK", NUM_VAL(WSAENOTSOCK));
#endif
#endif

#ifdef ESTRPIPE
    jml_module_add_value(module, "ESTRPIPE", NUM_VAL(ESTRPIPE));
#endif

#ifdef EMLINK
    jml_module_add_value(module, "EMLINK", NUM_VAL(EMLINK));
#endif

#ifdef ERANGE
    jml_module_add_value(module, "ERANGE", NUM_VAL(ERANGE));
#endif

#ifdef ELIBEXEC
    jml_module_add_value(module, "ELIBEXEC", NUM_VAL(ELIBEXEC));
#endif

#ifdef EL3HLT
    jml_module_add_value(module, "EL3HLT", NUM_VAL(EL3HLT));
#endif

#ifdef ECONNRESET
    jml_module_add_value(module, "ECONNRESET", NUM_VAL(ECONNRESET));
#else
#ifdef WSAECONNRESET
    jml_module_add_value(module, "WSAECONNRESET", NUM_VAL(WSAECONNRESET));
#endif
#endif

#ifdef EADDRINUSE
    jml_module_add_value(module, "EADDRINUSE", NUM_VAL(EADDRINUSE));
#else
#ifdef WSAEADDRINUSE
    jml_module_add_value(module, "WSAEADDRINUSE", NUM_VAL(WSAEADDRINUSE));
#endif
#endif

#ifdef EOPNOTSUPP
    jml_module_add_value(module, "EOPNOTSUPP", NUM_VAL(EOPNOTSUPP));
#else
#ifdef WSAEOPNOTSUPP
    jml_module_add_value(module, "WSAEOPNOTSUPP", NUM_VAL(WSAEOPNOTSUPP));
#endif
#endif

#ifdef EREMCHG
    jml_module_add_value(module, "EREMCHG", NUM_VAL(EREMCHG));
#endif

#ifdef EAGAIN
    jml_module_add_value(module, "EAGAIN", NUM_VAL(EAGAIN));
#endif

#ifdef ENAMETOOLONG
    jml_module_add_value(module, "ENAMETOOLONG", NUM_VAL(ENAMETOOLONG));
#else
#ifdef WSAENAMETOOLONG
    jml_module_add_value(module, "WSAENAMETOOLONG", NUM_VAL(WSAENAMETOOLONG));
#endif
#endif

#ifdef ENOTTY
    jml_module_add_value(module, "ENOTTY", NUM_VAL(ENOTTY));
#endif

#ifdef ERESTART
    jml_module_add_value(module, "ERESTART", NUM_VAL(ERESTART));
#endif

#ifdef ESOCKTNOSUPPORT
    jml_module_add_value(module, "ESOCKTNOSUPPORT", NUM_VAL(ESOCKTNOSUPPORT));
#else
#ifdef WSAESOCKTNOSUPPORT
    jml_module_add_value(module, "WSAESOCKTNOSUPPORT", NUM_VAL(WSAESOCKTNOSUPPORT));
#endif
#endif

#ifdef ETIME
    jml_module_add_value(module, "ETIME", NUM_VAL(ETIME));
#endif

#ifdef EBFONT
    jml_module_add_value(module, "EBFONT", NUM_VAL(EBFONT));
#endif

#ifdef EDEADLOCK
    jml_module_add_value(module, "EDEADLOCK", NUM_VAL(EDEADLOCK));
#endif

#ifdef ETOOMANYREFS
    jml_module_add_value(module, "ETOOMANYREFS", NUM_VAL(ETOOMANYREFS));
#else
#ifdef WSAETOOMANYREFS
    jml_module_add_value(module, "WSAETOOMANYREFS", NUM_VAL(WSAETOOMANYREFS));
#endif
#endif

#ifdef EMFILE
    jml_module_add_value(module, "EMFILE", NUM_VAL(EMFILE));
#else
#ifdef WSAEMFILE
    jml_module_add_value(module, "WSAEMFILE", NUM_VAL(WSAEMFILE));
#endif
#endif

#ifdef ETXTBSY
    jml_module_add_value(module, "ETXTBSY", NUM_VAL(ETXTBSY));
#endif

#ifdef EINPROGRESS
    jml_module_add_value(module, "EINPROGRESS", NUM_VAL(EINPROGRESS));
#else
#ifdef WSAEINPROGRESS
    jml_module_add_value(module, "WSAEINPROGRESS", NUM_VAL(WSAEINPROGRESS));
#endif
#endif

#ifdef ENXIO
    jml_module_add_value(module, "ENXIO", NUM_VAL(ENXIO));
#endif

#ifdef ENOPKG
    jml_module_add_value(module, "ENOPKG", NUM_VAL(ENOPKG));
#endif

#ifdef WSASY
    jml_module_add_value(module, "WSASY", NUM_VAL(WSASY));
#endif

#ifdef WSAEHOSTDOWN
    jml_module_add_value(module, "WSAEHOSTDOWN", NUM_VAL(WSAEHOSTDOWN));
#endif

#ifdef WSAENETDOWN
    jml_module_add_value(module, "WSAENETDOWN", NUM_VAL(WSAENETDOWN));
#endif

#ifdef WSAENOTSOCK
    jml_module_add_value(module, "WSAENOTSOCK", NUM_VAL(WSAENOTSOCK));
#endif

#ifdef WSAEHOSTUNREACH
    jml_module_add_value(module, "WSAEHOSTUNREACH", NUM_VAL(WSAEHOSTUNREACH));
#endif

#ifdef WSAELOOP
    jml_module_add_value(module, "WSAELOOP", NUM_VAL(WSAELOOP));
#endif

#ifdef WSAEMFILE
    jml_module_add_value(module, "WSAEMFILE", NUM_VAL(WSAEMFILE));
#endif

#ifdef WSAESTALE
    jml_module_add_value(module, "WSAESTALE", NUM_VAL(WSAESTALE));
#endif

#ifdef WSAVERNOTSUPPORTED
    jml_module_add_value(module, "WSAVERNOTSUPPORTED", NUM_VAL(WSAVERNOTSUPPORTED));
#endif

#ifdef WSAENETUNREACH
    jml_module_add_value(module, "WSAENETUNREACH", NUM_VAL(WSAENETUNREACH));
#endif

#ifdef WSAEPROCLIM
    jml_module_add_value(module, "WSAEPROCLIM", NUM_VAL(WSAEPROCLIM));
#endif

#ifdef WSAEFAULT
    jml_module_add_value(module, "WSAEFAULT", NUM_VAL(WSAEFAULT));
#endif

#ifdef WSANOTINITIALISED
    jml_module_add_value(module, "WSANOTINITIALISED", NUM_VAL(WSANOTINITIALISED));
#endif

#ifdef WSAEUSERS
    jml_module_add_value(module, "WSAEUSERS", NUM_VAL(WSAEUSERS));
#endif

#ifdef WSAMAKEASYNCREPL
    jml_module_add_value(module, "WSAMAKEASYNCREPL", NUM_VAL(WSAMAKEASYNCREPL));
#endif

#ifdef WSAENOPROTOOPT
    jml_module_add_value(module, "WSAENOPROTOOPT", NUM_VAL(WSAENOPROTOOPT));
#endif

#ifdef WSAECONNABORTED
    jml_module_add_value(module, "WSAECONNABORTED", NUM_VAL(WSAECONNABORTED));
#endif

#ifdef WSAENAMETOOLONG
    jml_module_add_value(module, "WSAENAMETOOLONG", NUM_VAL(WSAENAMETOOLONG));
#endif

#ifdef WSAENOTEMPTY
    jml_module_add_value(module, "WSAENOTEMPTY", NUM_VAL(WSAENOTEMPTY));
#endif

#ifdef WSAESHUTDOWN
    jml_module_add_value(module, "WSAESHUTDOWN", NUM_VAL(WSAESHUTDOWN));
#endif

#ifdef WSAEAFNOSUPPORT
    jml_module_add_value(module, "WSAEAFNOSUPPORT", NUM_VAL(WSAEAFNOSUPPORT));
#endif

#ifdef WSAETOOMANYREFS
    jml_module_add_value(module, "WSAETOOMANYREFS", NUM_VAL(WSAETOOMANYREFS));
#endif

#ifdef WSAEACCES
    jml_module_add_value(module, "WSAEACCES", NUM_VAL(WSAEACCES));
#endif

#ifdef WSATR
    jml_module_add_value(module, "WSATR", NUM_VAL(WSATR));
#endif

#ifdef WSABASEERR
    jml_module_add_value(module, "WSABASEERR", NUM_VAL(WSABASEERR));
#endif

#ifdef WSADESCRIPTIO
    jml_module_add_value(module, "WSADESCRIPTIO", NUM_VAL(WSADESCRIPTIO));
#endif

#ifdef WSAEMSGSIZE
    jml_module_add_value(module, "WSAEMSGSIZE", NUM_VAL(WSAEMSGSIZE));
#endif

#ifdef WSAEBADF
    jml_module_add_value(module, "WSAEBADF", NUM_VAL(WSAEBADF));
#endif

#ifdef WSAECONNRESET
    jml_module_add_value(module, "WSAECONNRESET", NUM_VAL(WSAECONNRESET));
#endif

#ifdef WSAGETSELECTERRO
    jml_module_add_value(module, "WSAGETSELECTERRO", NUM_VAL(WSAGETSELECTERRO));
#endif

#ifdef WSAETIMEDOUT
    jml_module_add_value(module, "WSAETIMEDOUT", NUM_VAL(WSAETIMEDOUT));
#endif

#ifdef WSAENOBUFS
    jml_module_add_value(module, "WSAENOBUFS", NUM_VAL(WSAENOBUFS));
#endif

#ifdef WSAEDISCON
    jml_module_add_value(module, "WSAEDISCON", NUM_VAL(WSAEDISCON));
#endif

#ifdef WSAEINTR
    jml_module_add_value(module, "WSAEINTR", NUM_VAL(WSAEINTR));
#endif

#ifdef WSAEPROTOTYPE
    jml_module_add_value(module, "WSAEPROTOTYPE", NUM_VAL(WSAEPROTOTYPE));
#endif

#ifdef WSAHOS
    jml_module_add_value(module, "WSAHOS", NUM_VAL(WSAHOS));
#endif

#ifdef WSAEADDRINUSE
    jml_module_add_value(module, "WSAEADDRINUSE", NUM_VAL(WSAEADDRINUSE));
#endif

#ifdef WSAEADDRNOTAVAIL
    jml_module_add_value(module, "WSAEADDRNOTAVAIL", NUM_VAL(WSAEADDRNOTAVAIL));
#endif

#ifdef WSAEALREADY
    jml_module_add_value(module, "WSAEALREADY", NUM_VAL(WSAEALREADY));
#endif

#ifdef WSAEPROTONOSUPPORT
    jml_module_add_value(module, "WSAEPROTONOSUPPORT", NUM_VAL(WSAEPROTONOSUPPORT));
#endif

#ifdef WSASYSNOTREADY
    jml_module_add_value(module, "WSASYSNOTREADY", NUM_VAL(WSASYSNOTREADY));
#endif

#ifdef WSAEWOULDBLOCK
    jml_module_add_value(module, "WSAEWOULDBLOCK", NUM_VAL(WSAEWOULDBLOCK));
#endif

#ifdef WSAEPFNOSUPPORT
    jml_module_add_value(module, "WSAEPFNOSUPPORT", NUM_VAL(WSAEPFNOSUPPORT));
#endif

#ifdef WSAEOPNOTSUPP
    jml_module_add_value(module, "WSAEOPNOTSUPP", NUM_VAL(WSAEOPNOTSUPP));
#endif

#ifdef WSAEISCONN
    jml_module_add_value(module, "WSAEISCONN", NUM_VAL(WSAEISCONN));
#endif

#ifdef WSAEDQUOT
    jml_module_add_value(module, "WSAEDQUOT", NUM_VAL(WSAEDQUOT));
#endif

#ifdef WSAENOTCONN
    jml_module_add_value(module, "WSAENOTCONN", NUM_VAL(WSAENOTCONN));
#endif

#ifdef WSAEREMOTE
    jml_module_add_value(module, "WSAEREMOTE", NUM_VAL(WSAEREMOTE));
#endif

#ifdef WSAEINVAL
    jml_module_add_value(module, "WSAEINVAL", NUM_VAL(WSAEINVAL));
#endif

#ifdef WSAEINPROGRESS
    jml_module_add_value(module, "WSAEINPROGRESS", NUM_VAL(WSAEINPROGRESS));
#endif

#ifdef WSAGETSELECTEVEN
    jml_module_add_value(module, "WSAGETSELECTEVEN", NUM_VAL(WSAGETSELECTEVEN));
#endif

#ifdef WSAESOCKTNOSUPPORT
    jml_module_add_value(module, "WSAESOCKTNOSUPPORT", NUM_VAL(WSAESOCKTNOSUPPORT));
#endif

#ifdef WSAGETASYNCERRO
    jml_module_add_value(module, "WSAGETASYNCERRO", NUM_VAL(WSAGETASYNCERRO));
#endif

#ifdef WSAMAKESELECTREPL
    jml_module_add_value(module, "WSAMAKESELECTREPL", NUM_VAL(WSAMAKESELECTREPL));
#endif

#ifdef WSAGETASYNCBUFLE
    jml_module_add_value(module, "WSAGETASYNCBUFLE", NUM_VAL(WSAGETASYNCBUFLE));
#endif

#ifdef WSAEDESTADDRREQ
    jml_module_add_value(module, "WSAEDESTADDRREQ", NUM_VAL(WSAEDESTADDRREQ));
#endif

#ifdef WSAECONNREFUSED
    jml_module_add_value(module, "WSAECONNREFUSED", NUM_VAL(WSAECONNREFUSED));
#endif

#ifdef WSAENETRESET
    jml_module_add_value(module, "WSAENETRESET", NUM_VAL(WSAENETRESET));
#endif

#ifdef WSAN
    jml_module_add_value(module, "WSAN", NUM_VAL(WSAN));
#endif

#ifdef ENOMEDIUM
    jml_module_add_value(module, "ENOMEDIUM", NUM_VAL(ENOMEDIUM));
#endif

#ifdef EMEDIUMTYPE
    jml_module_add_value(module, "EMEDIUMTYPE", NUM_VAL(EMEDIUMTYPE));
#endif

#ifdef ECANCELED
    jml_module_add_value(module, "ECANCELED", NUM_VAL(ECANCELED));
#endif

#ifdef ENOKEY
    jml_module_add_value(module, "ENOKEY", NUM_VAL(ENOKEY));
#endif

#ifdef EKEYEXPIRED
    jml_module_add_value(module, "EKEYEXPIRED", NUM_VAL(EKEYEXPIRED));
#endif

#ifdef EKEYREVOKED
    jml_module_add_value(module, "EKEYREVOKED", NUM_VAL(EKEYREVOKED));
#endif

#ifdef EKEYREJECTED
    jml_module_add_value(module, "EKEYREJECTED", NUM_VAL(EKEYREJECTED));
#endif

#ifdef EOWNERDEAD
    jml_module_add_value(module, "EOWNERDEAD", NUM_VAL(EOWNERDEAD));
#endif

#ifdef ENOTRECOVERABLE
    jml_module_add_value(module, "ENOTRECOVERABLE", NUM_VAL(ENOTRECOVERABLE));
#endif

#ifdef ERFKILL
    jml_module_add_value(module, "ERFKILL", NUM_VAL(ERFKILL));
#endif

#ifdef ECANCELED
    jml_module_add_value(module, "ECANCELED", NUM_VAL(ECANCELED));
#endif

#ifdef ENOTSUP
    jml_module_add_value(module, "ENOTSUP", NUM_VAL(ENOTSUP));
#endif

#ifdef EOWNERDEAD
    jml_module_add_value(module, "EOWNERDEAD", NUM_VAL(EOWNERDEAD));
#endif

#ifdef ENOTRECOVERABLE
    jml_module_add_value(module, "ENOTRECOVERABLE", NUM_VAL(ENOTRECOVERABLE));
#endif

#ifdef ELOCKUNMAPPED
    jml_module_add_value(module, "ELOCKUNMAPPED", NUM_VAL(ELOCKUNMAPPED));
#endif

#ifdef ENOTACTIVE
    jml_module_add_value(module, "ENOTACTIVE", NUM_VAL(ENOTACTIVE));
#endif

#ifdef EAUTH
    jml_module_add_value(module, "EAUTH", NUM_VAL(EAUTH));
#endif

#ifdef EBADARCH
    jml_module_add_value(module, "EBADARCH", NUM_VAL(EBADARCH));
#endif

#ifdef EBADEXEC
    jml_module_add_value(module, "EBADEXEC", NUM_VAL(EBADEXEC));
#endif

#ifdef EBADMACHO
    jml_module_add_value(module, "EBADMACHO", NUM_VAL(EBADMACHO));
#endif

#ifdef EBADRPC
    jml_module_add_value(module, "EBADRPC", NUM_VAL(EBADRPC));
#endif

#ifdef EDEVERR
    jml_module_add_value(module, "EDEVERR", NUM_VAL(EDEVERR));
#endif

#ifdef EFTYPE
    jml_module_add_value(module, "EFTYPE", NUM_VAL(EFTYPE));
#endif

#ifdef ENEEDAUTH
    jml_module_add_value(module, "ENEEDAUTH", NUM_VAL(ENEEDAUTH));
#endif

#ifdef ENOATTR
    jml_module_add_value(module, "ENOATTR", NUM_VAL(ENOATTR));
#endif

#ifdef ENOPOLICY
    jml_module_add_value(module, "ENOPOLICY", NUM_VAL(ENOPOLICY));
#endif

#ifdef EPROCLIM
    jml_module_add_value(module, "EPROCLIM", NUM_VAL(EPROCLIM));
#endif

#ifdef EPROCUNAVAIL
    jml_module_add_value(module, "EPROCUNAVAIL", NUM_VAL(EPROCUNAVAIL));
#endif

#ifdef EPROGMISMATCH
    jml_module_add_value(module, "EPROGMISMATCH", NUM_VAL(EPROGMISMATCH));
#endif

#ifdef EPROGUNAVAIL
    jml_module_add_value(module, "EPROGUNAVAIL", NUM_VAL(EPROGUNAVAIL));
#endif

#ifdef EPWROFF
    jml_module_add_value(module, "EPWROFF", NUM_VAL(EPWROFF));
#endif

#ifdef ERPCMISMATCH
    jml_module_add_value(module, "ERPCMISMATCH", NUM_VAL(ERPCMISMATCH));
#endif

#ifdef ESHLIBVERS
    jml_module_add_value(module, "ESHLIBVERS", NUM_VAL(ESHLIBVERS));
#endif
}
