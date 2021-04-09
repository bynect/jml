#ifndef JML_SIGNAL_H_
#define JML_SIGNAL_H_


#include <jml.h>


typedef struct {
    const char *name;
    jml_obj_t *handler;
} jml_signal_t;


void jml_signal_init(void);

void jml_signal_set(int signum, jml_obj_t *handler);

void jml_signal_unset(int signum);


extern jml_signal_t jml_signals[];


#endif /* JML_SIGNAL_H_ */
