#include <signal.h>
#include <stdlib.h>

#include <jml.h>
#include <jml/jml_signal.h>


static jml_value_t
jml_std_signal_signal(int arg_count, jml_value_t *args)
{
jml_obj_exception_t *exc = jml_error_args(
        arg_count, 2);

    if (exc != NULL)
        goto err;

    if (!IS_NUM(args[0]) && !IS_OBJ(args[1]) && !IS_NUM(args[1])) {
        exc = jml_error_types(false, 2, "number", "handler");
        goto err;
    }

    int signum          = AS_NUM(args[0]);
    intptr_t old        = -1;

    if (IS_NUM(args[1])) {
        intptr_t handler = AS_NUM(args[1]);

        switch (handler) {
            case (intptr_t)SIG_DFL:
                jml_signal_unset(signum);
                old = 0;
                break;

            case (intptr_t)SIG_IGN:
                jml_signal_unset(signum);
                old = (intptr_t)signal(signum, SIG_IGN);
                break;

#ifdef SIG_HOLD
            case (intptr_t)SIG_HOLD:
                jml_signal_unset(signum);
                old = (intptr_t)signal(signum, SIG_HOLD);
                break;
#endif
            default:
                break;
        }
    } else if (IS_CFUNCTION(args[1])) {
        jml_signal_set(signum, AS_OBJ(args[1]));
        old = 0;

    } else {
        exc = jml_error_implemented(args[1]);
        goto err;
    }

    if (old == -1) {
        exc = jml_obj_exception_new("WrongValue", "Invalid signal handler.");
        goto err;
    }

    return NONE_VAL;

err:
    return OBJ_VAL(exc);
}


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"signal",                      &jml_std_signal_signal},
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

    /*default handlers*/
    jml_module_add_value(module, "SIG_DFL", NUM_VAL((intptr_t)SIG_DFL));
    jml_module_add_value(module, "SIG_IGN", NUM_VAL((intptr_t)SIG_IGN));

#ifdef SIG_HOLD
    jml_module_add_value(module, "SIG_HOLD", NUM_VAL((intptr_t)SIG_HOLD));
#endif
}
