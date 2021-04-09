#include <signal.h>
#include <stdlib.h>

#include <jml.h>
#include <jml/jml_signal.h>


/*signal table*/
#define SIGNAL_ENTRY(sig, handler)                      \
    [sig] = {#sig,                  handler}


jml_signal_t jml_signals[] = {
#ifdef SIGINT
    SIGNAL_ENTRY(SIGINT, NULL),
#endif

#ifdef SIGILL
    SIGNAL_ENTRY(SIGILL, NULL),
#endif

#ifdef SIGABRT
    SIGNAL_ENTRY(SIGABRT, NULL),
#endif

#ifdef SIGFPE
    SIGNAL_ENTRY(SIGFPE, NULL),
#endif

#ifdef SIGSEGV
    SIGNAL_ENTRY(SIGSEGV, NULL),
#endif

#ifdef SIGTERM
    SIGNAL_ENTRY(SIGTERM, NULL),
#endif

#ifdef SIGHUP
    SIGNAL_ENTRY(SIGHUP, NULL),
#endif

#ifdef SIGQUIT
    SIGNAL_ENTRY(SIGQUIT, NULL),
#endif

#ifdef SIGTRAP
    SIGNAL_ENTRY(SIGTRAP, NULL),
#endif

#ifdef SIGKILL
    SIGNAL_ENTRY(SIGKILL, NULL),
#endif

#ifdef SIGBUS
    SIGNAL_ENTRY(SIGBUS, NULL),
#endif

#ifdef SIGSYS
    SIGNAL_ENTRY(SIGSYS, NULL),
#endif

#ifdef SIGPIPE
    SIGNAL_ENTRY(SIGPIPE, NULL),
#endif

#ifdef SIGALRM
    SIGNAL_ENTRY(SIGALRM, NULL),
#endif

#ifdef SIGURG
    SIGNAL_ENTRY(SIGURG, NULL),
#endif

#ifdef SIGSTOP
    SIGNAL_ENTRY(SIGSTOP, NULL),
#endif

#ifdef SIGTSTP
    SIGNAL_ENTRY(SIGTSTP, NULL),
#endif

#ifdef SIGCONT
    SIGNAL_ENTRY(SIGCONT, NULL),
#endif

#ifdef SIGCHLD
    SIGNAL_ENTRY(SIGCHLD, NULL),
#endif

#ifdef SIGTTIN
    SIGNAL_ENTRY(SIGTTIN, NULL),
#endif

#ifdef SIGTTOU
    SIGNAL_ENTRY(SIGTTOU, NULL),
#endif

#ifdef SIGPOLL
    SIGNAL_ENTRY(SIGPOLL, NULL),
#endif

#ifdef SIGXCPU
    SIGNAL_ENTRY(SIGXCPU, NULL),
#endif

#ifdef SIGXFSZ
    SIGNAL_ENTRY(SIGXFSZ, NULL),
#endif

#ifdef SIGVTALRM
    SIGNAL_ENTRY(SIGVTALRM, NULL),
#endif

#ifdef SIGPROF
    SIGNAL_ENTRY(SIGPROF, NULL),
#endif

#ifdef SIGUSR1
    SIGNAL_ENTRY(SIGUSR1, NULL),
#endif

#ifdef SIGUSR2
    SIGNAL_ENTRY(SIGUSR2, NULL),
#endif

#ifdef SIGWINCH
    SIGNAL_ENTRY(SIGWINCH, NULL),
#endif

#ifdef SIGEMT
    SIGNAL_ENTRY(SIGEMT, NULL),
#endif

#ifdef SIGPWR
    SIGNAL_ENTRY(SIGPWR, NULL),
#endif

#ifdef SIGLOST
    SIGNAL_ENTRY(SIGLOST, NULL),
#endif

#ifdef SIGSTKFLT
    SIGNAL_ENTRY(SIGSTKFLT, NULL),
#endif

#ifdef SIGUNUSED
    SIGNAL_ENTRY(SIGUNUSED, NULL),
#endif

#ifdef SIGTHR
    SIGNAL_ENTRY(SIGTHR, NULL),
#endif

#ifdef SIGLIBRT
    SIGNAL_ENTRY(SIGLIBRT, NULL),
#endif

#ifdef SIGBREAK
    SIGNAL_ENTRY(SIGBREAK, NULL),
#endif

#if defined SIGIO && !defined SIGPOLL
    SIGNAL_ENTRY(SIGIO, NULL),
#endif

#if defined SIGIOT && !defined SIGABRT
    SIGNAL_ENTRY(SIGIOT, NULL),
#endif

#if defined SIGCLD && !defined SIGCHLD
    SIGNAL_ENTRY(SIGCLD, NULL),
#endif
};


static void
jml_signal_handler(int signum)
{
    signal(signum, jml_signal_handler);

    jml_obj_t *handler = jml_signals[signum].handler;
    if (handler == NULL)
        return;

    switch (handler->type) {
        case OBJ_CFUNCTION: {
            jml_value_t args = NUM_VAL(signum);
            ((jml_obj_cfunction_t*)handler)->function(1, &args);
            break;
        }

        default:
            return;
    }
}


void
jml_signal_init(void)
{
    static bool initialized = 0;
    if (initialized) return;

    uint8_t length = sizeof(jml_signals) / sizeof(jml_signal_t);
    for (uint8_t i = 0; i < length; ++i) {
        if (jml_signals[i].name == NULL)
            continue;
        signal(i, jml_signals[i].handler == NULL ? SIG_DFL : jml_signal_handler);
    }
    initialized = 1;
}


void
jml_signal_set(int signum, jml_obj_t *handler)
{
    jml_signals[signum].handler = handler;
    signal(signum, jml_signal_handler);
}


void
jml_signal_unset(int signum)
{
    jml_signals[signum].handler = NULL;
    signal(signum, SIG_DFL);
}
