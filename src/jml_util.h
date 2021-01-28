#ifndef JML_UTIL_H_
#define JML_UTIL_H_

#include <jml_common.h>


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


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#include <limits.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#define JML_PATH_MAX                PATH_MAX


static inline bool
jml_file_exist(const char *filename)
{
    return access(filename, F_OK) != -1;
}


static inline bool
jml_file_isdir(const char *path)
{
    struct stat path_stat;
    if (stat(path, &path_stat) < 0)
        return false;

    return S_ISDIR(path_stat.st_mode);
}


#elif defined JML_PLATFORM_WIN

#define JML_PATH_MAX                260


static inline bool
jml_file_exist(const char *filename)
{
    (void) filename;
    return false;
}


static inline bool
jml_file_isdir(const char *path)
{
    (void) path;
    return false;
}


#endif

bool jml_file_find(const char *filename, char *result);

bool jml_file_find_in(const char *path,
    const char *filename, char *result);

char *jml_file_read(const char *path);


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
jml_is_print(char c)
{
    return (c >= ' ' && c <= '~');
}


#endif /* JML_UTIL_H_ */
