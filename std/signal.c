#include <signal.h>
#include <stdlib.h>

#include <jml.h>


/*placeholder*/
static jml_value_t
jml_std_signal_sigdfl(JML_UNUSED(int arg_count),
    JML_UNUSED(jml_value_t *args))
{
    return NONE_VAL;
}


static jml_value_t
jml_std_signal_sigign(JML_UNUSED(int arg_count),
    JML_UNUSED(jml_value_t *args))
{
    return NONE_VAL;
}


static jml_value_t
jml_std_signal_sighold(JML_UNUSED(int arg_count),
    JML_UNUSED(jml_value_t *args))
{
    return NONE_VAL;
}


static jml_cfunction jml_signal_handlers[] = {
#ifdef SIGINT
    [SIGINT] = NULL,
#endif

#ifdef SIGILL
    [SIGILL] = NULL,
#endif

#ifdef SIGABRT
    [SIGABRT] = NULL,
#endif

#ifdef SIGFPE
    [SIGFPE] = NULL,
#endif

#ifdef SIGSEGV
    [SIGSEGV] = NULL,
#endif

#ifdef SIGTERM
    [SIGTERM] = NULL,
#endif

#ifdef SIGHUP
    [SIGHUP] = NULL,
#endif

#ifdef SIGQUIT
    [SIGQUIT] = NULL,
#endif

#ifdef SIGTRAP
    [SIGTRAP] = NULL,
#endif

#ifdef SIGKILL
    [SIGKILL] = NULL,
#endif

#ifdef SIGBUS
    [SIGBUS] = NULL,
#endif

#ifdef SIGSYS
    [SIGSYS] = NULL,
#endif

#ifdef SIGPIPE
    [SIGPIPE] = NULL,
#endif

#ifdef SIGALRM
    [SIGALRM] = NULL,
#endif

#ifdef SIGURG
    [SIGURG] = NULL,
#endif

#ifdef SIGSTOP
    [SIGSTOP] = NULL,
#endif

#ifdef SIGTSTP
    [SIGTSTP] = NULL,
#endif

#ifdef SIGCONT
    [SIGCONT] = NULL,
#endif

#ifdef SIGCHLD
    [SIGCHLD] = NULL,
#endif

#ifdef SIGTTIN
    [SIGTTIN] = NULL,
#endif

#ifdef SIGTTOU
    [SIGTTOU] = NULL,
#endif

#ifdef SIGPOLL
    [SIGPOLL] = NULL,
#endif

#ifdef SIGXCPU
    [SIGXCPU] = NULL,
#endif

#ifdef SIGXFSZ
    [SIGXFSZ] = NULL,
#endif

#ifdef SIGVTALRM
    [SIGVTALRM] = NULL,
#endif

#ifdef SIGPROF
    [SIGPROF] = NULL,
#endif

#ifdef SIGUSR1
    [SIGUSR1] = NULL,
#endif

#ifdef SIGUSR2
    [SIGUSR2] = NULL,
#endif

#ifdef SIGWINCH
    [SIGWINCH] = NULL,
#endif

#ifdef SIGEMT
    [SIGEMT] = NULL,
#endif

#ifdef SIGPWR
    [SIGPWR] = NULL,
#endif

#ifdef SIGLOST
    [SIGLOST] = NULL,
#endif

#ifdef SIGSTKFLT
    [SIGSTKFLT] = NULL,
#endif

#ifdef SIGUNUSED
    [SIGUNUSED] = NULL,
#endif

#ifdef SIGTHR
    [SIGTHR] = NULL,
#endif

#ifdef SIGLIBRT
    [SIGLIBRT] = NULL,
#endif

#ifdef SIGBREAK
    [SIGBREAK] = NULL,
#endif

#if defined SIGIO && !defined SIGPOLL
    [SIGIO] = NULL,
#endif

#if defined SIGIOT && !defined SIGABRT
    [SIGIOT] = NULL,
#endif

#if defined SIGCLD && !defined SIGCHLD
    [SIGCLD] = NULL,
#endif
};


static void
jml_signal_handler(int signum)
{
    signal(signum, jml_signal_handler);

    jml_cfunction handler = jml_signal_handlers[signum];

    if (handler != NULL) {
        jml_value_t arg = NUM_VAL(signum);
        handler(1, &arg);
    }
}


static jml_value_t
jml_std_signal_signal(int arg_count, jml_value_t *args)
{
    jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0]) && !IS_OBJ(args[1])) {
        exc = jml_error_types(false, 2, "number", "handler");
        goto err;
    }

    int signum          = AS_NUM(args[0]);
    const int sigsize   = sizeof(jml_signal_handlers) / sizeof(jml_value_t);

    if (signum < 0 || signum > sigsize) {
        exc = jml_error_value("signal");
        goto err;
    }

    if (IS_CFUNCTION(args[1])) {
        jml_cfunction handler = AS_CFUNCTION(args[1])->function;

        if (handler == jml_std_signal_sigdfl)
            signal(signum, SIG_DFL);
        else if (handler == jml_std_signal_sigign)
            signal(signum, SIG_IGN);
        else if (handler == jml_std_signal_sighold) {
#ifdef SIG_HOLD

            signal(signum, SIG_HOLD);

#endif
        } else {
            jml_signal_handlers[signum] = handler;
            signal(signum, jml_signal_handler);
        }

        return NONE_VAL;
    }

    return OBJ_VAL(jml_error_implemented(args[1]));

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"signal",                      &jml_std_signal_signal},
    {"SIG_DFL",                     &jml_std_signal_sigdfl},
    {"SIG_IGN",                     &jml_std_signal_sigign},

#ifdef SIG_HOLD

    {"SIG_HOLD",                    &jml_std_signal_sighold},

#endif

    {NULL,                          NULL}
};


MODULE_FUNC_HEAD
module_init(jml_obj_module_t *module)
{
#ifdef SIGINT
    jml_module_add_value(module, "SIGINT", NUM_VAL(SIGINT));
#endif

#ifdef SIGILL
    jml_module_add_value(module, "SIGILL", NUM_VAL(SIGILL));
#endif

#ifdef SIGABRT
    jml_module_add_value(module, "SIGABRT", NUM_VAL(SIGABRT));
#endif

#ifdef SIGFPE
    jml_module_add_value(module, "SIGFPE", NUM_VAL(SIGFPE));
#endif

#ifdef SIGSEGV
    jml_module_add_value(module, "SIGSEGV", NUM_VAL(SIGSEGV));
#endif

#ifdef SIGTERM
    jml_module_add_value(module, "SIGTERM", NUM_VAL(SIGTERM));
#endif

#ifdef SIGHUP
    jml_module_add_value(module, "SIGHUP", NUM_VAL(SIGHUP));
#endif

#ifdef SIGQUIT
    jml_module_add_value(module, "SIGQUIT", NUM_VAL(SIGQUIT));
#endif

#ifdef SIGTRAP
    jml_module_add_value(module, "SIGTRAP", NUM_VAL(SIGTRAP));
#endif

#ifdef SIGKILL
    jml_module_add_value(module, "SIGKILL", NUM_VAL(SIGKILL));
#endif

#ifdef SIGBUS
    jml_module_add_value(module, "SIGBUS", NUM_VAL(SIGBUS));
#endif

#ifdef SIGSYS
    jml_module_add_value(module, "SIGSYS", NUM_VAL(SIGSYS));
#endif

#ifdef SIGPIPE
    jml_module_add_value(module, "SIGPIPE", NUM_VAL(SIGPIPE));
#endif

#ifdef SIGALRM
    jml_module_add_value(module, "SIGALRM", NUM_VAL(SIGALRM));
#endif

#ifdef SIGURG
    jml_module_add_value(module, "SIGURG", NUM_VAL(SIGURG));
#endif

#ifdef SIGSTOP
    jml_module_add_value(module, "SIGSTOP", NUM_VAL(SIGSTOP));
#endif

#ifdef SIGTSTP
    jml_module_add_value(module, "SIGTSTP", NUM_VAL(SIGTSTP));
#endif

#ifdef SIGCONT
    jml_module_add_value(module, "SIGCONT", NUM_VAL(SIGCONT));
#endif

#ifdef SIGCHLD
    jml_module_add_value(module, "SIGCHLD", NUM_VAL(SIGCHLD));
#endif

#ifdef SIGTTIN
    jml_module_add_value(module, "SIGTTIN", NUM_VAL(SIGTTIN));
#endif

#ifdef SIGTTOU
    jml_module_add_value(module, "SIGTTOU", NUM_VAL(SIGTTOU));
#endif

#ifdef SIGPOLL
    jml_module_add_value(module, "SIGPOLL", NUM_VAL(SIGPOLL));
#endif

#ifdef SIGXCPU
    jml_module_add_value(module, "SIGXCPU", NUM_VAL(SIGXCPU));
#endif

#ifdef SIGXFSZ
    jml_module_add_value(module, "SIGXFSZ", NUM_VAL(SIGXFSZ));
#endif

#ifdef SIGVTALRM
    jml_module_add_value(module, "SIGVTALRM", NUM_VAL(SIGVTALRM));
#endif

#ifdef SIGPROF
    jml_module_add_value(module, "SIGPROF", NUM_VAL(SIGPROF));
#endif

#ifdef SIGUSR1
    jml_module_add_value(module, "SIGUSR1", NUM_VAL(SIGUSR1));
#endif

#ifdef SIGUSR2
    jml_module_add_value(module, "SIGUSR2", NUM_VAL(SIGUSR2));
#endif

#ifdef SIGWINCH
    jml_module_add_value(module, "SIGWINCH", NUM_VAL(SIGWINCH));
#endif

#ifdef SIGIO
    jml_module_add_value(module, "SIGIO", NUM_VAL(SIGIO));
#endif

#ifdef SIGIOT
    jml_module_add_value(module, "SIGIOT", NUM_VAL(SIGIOT));
#endif

#ifdef SIGCLD
    jml_module_add_value(module, "SIGCLD", NUM_VAL(SIGCLD));
#endif

#ifdef SIGEMT
    jml_module_add_value(module, "SIGEMT", NUM_VAL(SIGEMT));
#endif

#ifdef SIGPWR
    jml_module_add_value(module, "SIGPWR", NUM_VAL(SIGPWR));
#endif

#ifdef SIGLOST
    jml_module_add_value(module, "SIGLOST", NUM_VAL(SIGLOST));
#endif

#ifdef SIGSTKFLT
    jml_module_add_value(module, "SIGSTKFLT", NUM_VAL(SIGSTKFLT));
#endif

#ifdef SIGUNUSED
    jml_module_add_value(module, "SIGUNUSED", NUM_VAL(SIGUNUSED));
#endif

#ifdef SIGTHR
    jml_module_add_value(module, "SIGTHR", NUM_VAL(SIGTHR));
#endif

#ifdef SIGLIBRT
    jml_module_add_value(module, "SIGLIBRT", NUM_VAL(SIGLIBRT));
#endif

#ifdef SIGBREAK
    jml_module_add_value(module, "SIGBREAK", NUM_VAL(SIGBREAK));
#endif
}
