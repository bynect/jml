#include <string.h>
#include <stdio.h>

#include <jml.h>

#include <jml_util.h>
#include <jml_gc.h>


#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#elif JML_PLATFORM_WIN

#include <windows.h>

#endif


char *
jml_strsep(char **str, const char *sep)
{
    char *ptr = *str, *end;
    if (!ptr)
        return NULL;

    end = ptr + strcspn(ptr, sep);

    if (*end)
        *end++ = 0;
    else
        end = 0;

    *str = end;
    return ptr;
}


char *
jml_strtok(char *str, const char *delim,
    char **save)
{
    if (str == NULL) {
        if (*save == NULL)
            return NULL;
        str = *save;
    }

    if (*str == '\0') {
        *save = str;
        return NULL;
    }

    char *end = strstr(str, delim);
    if (end == NULL) {
        *save = end;
        return str;
    }

    *end = '\0';
    *save = end + strlen(delim);
    return str;
}


char *
jml_strdup(const char *str)
{
    size_t length = strlen(str) + 1;
    char *dest = jml_realloc(NULL,
        length);

    if (!dest) return NULL;

    return memcpy(dest, str, length);
}


char *
jml_strcat(char *dest, char *src)
{
    while (*dest) dest++;
    while ((*dest++ = *src++));

    return --dest;
}


bool
jml_file_find(const char *filename, char *result)
{
#ifdef DT_DIR
#define IS_DIR                      DT_DIR
#else
#define IS_DIR                      4
#endif
    DIR *dp    = opendir(".");
    if (dp == NULL) return false;

    struct dirent *dir;
    while ((dir = readdir(dp))) {

        if (strcmp(dir->d_name, ".") == 0
            || strcmp(dir->d_name, "..") == 0)
            continue;

        if (dir->d_type == IS_DIR) {
            if (chdir(dir->d_name) != 0)
                break;

            jml_file_find(filename, result);

            if (chdir("..") != 0)
                break;

        } else if (strcmp(dir->d_name, filename) == 0) {
            getcwd(result, JML_PATH_MAX);
            int length = strlen(result);
            snprintf(result + length, JML_PATH_MAX - length, "/%s", dir->d_name);
            break;
        }
    }

    closedir(dp);
    return (*result != 0);

#undef IS_DIR
}


bool
jml_file_find_in(const char *path,
    const char *filename, char *result)
{
    bool res;
    char *temp = getcwd(NULL, 0);

    if (chdir(path) != 0) {
        res = false;
        goto done;
    }

    res = jml_file_find(filename, result);

    if (chdir(temp) != 0){
        res = false;
        goto done;
    }

done:
    jml_free(temp);
    return res;
}


bool
jml_file_exist(const char *filename)
{
#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

    return access(filename, F_OK) != -1;
#elif JML_PLATFORM_WIN
    (void) filename;
    return false;
#endif
}


bool
jml_file_isdir(const char *path)
{
#if defined JML_PLATFORM_NIX || defined JML_PLATFORM_MAC

    struct stat path_stat;
    if (stat(path, &path_stat) < 0)
        return false;

    return S_ISDIR(path_stat.st_mode);
#elif JML_PLATFORM_WIN
    (void) path;
    return false;
#endif
}


char *
jml_file_read(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return NULL;

    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char *buffer = jml_realloc(NULL, size + 1);
    if (buffer == NULL)
        return NULL;

    size_t read = fread(buffer, sizeof(char), size, file);
    if (read < size)
        return NULL;

    buffer[read] = '\0';
    fclose(file);

    return buffer;
}
