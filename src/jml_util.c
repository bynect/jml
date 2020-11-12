#include <string.h>

#include <jml_util.h>
#include <jml_gc.h>


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
    char *dest = (char*)jml_realloc(NULL,
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
