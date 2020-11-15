#ifndef JML_UTIL_H_
#define JML_UTIL_H_

#include <jml.h>

#include <jml_common.h>


char *jml_strsep(char **str, const char *sep);

char *jml_strtok(char *input, const char *delimiter);

char *jml_strdup(const char *str);

char *jml_strcat(char *dest, char *src);


static inline bool
jml_strprfx(const char *pre,
    const char *str, size_t length)
{
    return strncmp(pre, str, length) == 0;
}


static inline bool
jml_strsfx(const char *suf,
    const char *str, size_t suflen, size_t strlen)
{
    if (suflen > strlen) return false;
    return strncmp(str + strlen - suflen, suf, suflen) == 0;
}


#ifdef JML_PLATFORM_NIX

#include <unistd.h>
#include <limits.h>
#include <sys/param.h>

#define JML_PATH_MAX                PATH_MAX


static inline bool
jml_file_exist(const char *filename)
{
    return access(filename, F_OK) != -1;
}

#elif JML_PLATFORM_WIN

#define JML_PATH_MAX                260

#endif

bool jml_file_find(char *filename, char *result);

char *jml_file_read(const char *path);


#endif /* JML_UTIL_H_ */
