#ifndef JML_UTIL_H_
#define JML_UTIL_H_

#include <jml.h>

#include <jml_common.h>


char *jml_strsep(char **str, const char *sep);

char *jml_strtok(char *input, const char *delimiter);

char *jml_strdup(const char *str);

char *jml_strcat(char *dest, char *src);


#endif /* JML_UTIL_H_ */
