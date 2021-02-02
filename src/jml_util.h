#ifndef JML_UTIL_H_
#define JML_UTIL_H_

#include <jml_common.h>


char *jml_strsep(char **str, const char *sep);

char *jml_strtok(char *str, const char *delim, char **save);

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


static inline bool
jml_is_digit(char c)
{
    return (c >= '0' && c <= '9');
}


static inline bool
jml_is_hex(char c)
{
    return (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F')
        || (c >= '0' && c <= '9');
}


static inline bool
jml_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c == '_');
}


static inline bool
jml_is_bracket(char c)
{
    return (c == '(' || c == ')')
        || (c == '[' || c == ']')
        || (c == '{' || c == '}');
}


static inline bool
jml_is_control(char c)
{
    return (c == 127)
        || (c >= 0 && c <= 31);
}


static inline bool
jml_is_printable(char c)
{
    return (c >= ' ' && c <= '~');
}


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#define JML_PATH_MAX                4096
#define PATH_SEPARATOR              '/'

#elif defined JML_PLATFORM_WIN

#define JML_PATH_MAX                260
#define PATH_SEPARATOR              '\\'

#endif


bool jml_file_find(const char *filename, char *result);

bool jml_file_find_in(const char *path,
    const char *filename, char *result);

bool jml_file_exist(const char *path);

bool jml_file_isdir(const char *path);

char *jml_file_read(const char *path);


#endif /* JML_UTIL_H_ */
