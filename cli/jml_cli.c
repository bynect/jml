#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <jml.h>


static char *
jml_cli_fread(const char *path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file '%s'.\n", path);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char *buffer = jml_realloc(NULL, size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read '%s'.\n", path);
        exit(EXIT_FAILURE);
    }

    size_t read = fread(buffer, sizeof(char), size, file);
    if (read < size) {
        fprintf(stderr, "Could not read file '%s'.\n", path);
        exit(EXIT_FAILURE);
    }

    buffer[read] = '\0';

    fclose(file);
    return buffer;
}


static void
jml_cli_run(const char *path)
{
    char *source = jml_cli_fread(path);
    jml_interpret_result result = jml_vm_interpret(source);
    free(source);

    if (result != INTERPRET_OK)
        exit(EXIT_FAILURE);
}


static void
jml_cli_repl(void)
{
    signal(SIGINT, SIG_IGN);

    printf(
        "interactive jml %s (on %s)\n",
        JML_VERSION_STRING,
        JML_PLATFORM_STRING
    );

    char line[2048];
    for ( ;; ) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        if (strcmp(line, "\n") == 0)
            continue;

#ifdef JML_EVAL
        jml_value_t result = jml_vm_eval(line);

        if (!jml_obj_is_sentinel(result) && !IS_NONE(result)) {
            jml_value_print(result);
            printf("\n");
        }
#else
    jml_vm_interpret(line);
#endif
    }
}


int
main(int argc, char **argv)
{
    jml_vm_t *vm = jml_vm_new();

    if (argc == 1)
        jml_cli_repl();

    else if (argc == 2)
        jml_cli_run(argv[1]);

    else {
        printf("%s\n", "Usage: jml [file]");
        return EXIT_FAILURE;
    }

    jml_vm_free(vm);

    return EXIT_SUCCESS;
}
