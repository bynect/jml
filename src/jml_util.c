#include <string.h>
#include <stdio.h>

#include <jml.h>

#include <jml_util.h>
#include <jml_gc.h>


#ifdef JML_PLATFORM_NIX

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#elif JML_PLATFORM_WIN

#include <windows.h>

#endif


char *
jml_strsep(char **str, const char *sep)
{
    char *s = *str, *end;
    if (!s) return NULL;
    end = s + strcspn(s, sep);

    if (*end) *end++ = 0;
    else end = 0;

    *str = end;
    return s;
}


char *
jml_strtok(char *input,
    const char *delimiter)
{
    static char *string;
    if (input != NULL)
        string = input;
    if (string == NULL)
        return string;

    char *end = strstr(string, delimiter);
    if (end == NULL) {
        char *temp = string;
        string = NULL;
        return temp;
    }

    char *temp = string;
    *end = '\0';
    string = end + strlen(delimiter);
    return temp;
}


char *
jml_strdup(const char *str)
{
    size_t length = strlen(str) + 1;
    char *dest = jml_realloc(NULL,
        length);

    if (!dest) return NULL;

    return (char*)memcpy(dest, str,
        length);
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

            chdir( dir->d_name );
            jml_file_find( filename, result );
            chdir( ".." );
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
    char *tmp = getcwd(NULL, 0);
    chdir(path);
    
    bool res = jml_file_find(filename, result);

    chdir(tmp);
    jml_free(tmp);

    return res;
}


char *
jml_file_read(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;

    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char *buffer = jml_realloc(NULL, size + 1);
    if (buffer == NULL) return NULL;

    size_t read = fread(buffer, sizeof(char), size, file);
    if (read < size) return NULL;

    buffer[read] = '\0';
    fclose(file);

    return buffer;
}
