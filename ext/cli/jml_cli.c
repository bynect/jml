#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <jml.h>


#ifdef JML_CLI_READLINE

#include <readline/readline.h>
#include <readline/history.h>

#endif


#define print_error(message, ...)                       \
    do {                                                \
        fprintf(stderr, message, __VA_ARGS__);          \
        exit(EXIT_FAILURE);                             \
    } while (false)


static char *
jml_cli_fread(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        print_error("Could not open file '%s'.\n", path);

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char *buffer = jml_realloc(NULL, size + 1);
    if (buffer == NULL)
        print_error("Not enough memory to read '%s'.\n", path);

    size_t bytes = fread(buffer, sizeof(char), size, file);
    if (bytes < size)
        print_error("Could not read file '%s'.\n", path);

    buffer[bytes] = '\0';

    fclose(file);
    return buffer;
}


static bool
jml_cli_run(jml_vm_t *vm, const char *path)
{
    char *source = jml_cli_fread(path);
    jml_interpret_result result = jml_vm_interpret(vm, source);
    free(source);

    return result == INTERPRET_OK;
}


static void
jml_cli_repl(jml_vm_t *vm)
{
    signal(SIGINT, SIG_IGN);

    printf(
        "interactive jml -- v%s (on %s)\n",
        JML_VERSION_STRING,
        JML_PLATFORM_STRING
    );

#ifdef JML_CLI_READLINE
    char *line;
    while ((line = readline("~> ")) != NULL) {
        if (strlen(line) > 0)
            add_history(line);
        else
            continue;
#else
    char line[2048];
    while (true) {
        printf("~> ");

        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        if (*line == '\n')
            continue;
#endif

#ifdef JML_EVAL
        jml_value_t result = jml_vm_eval(vm, line);

        if (!IS_EXCEPTION(result) && !IS_NONE(result)) {
            printf("   ");
            jml_value_print(result);
            printf("\n");
        }
#else
        jml_vm_interpret(vm, line);
#endif

#ifdef JML_CLI_READLINE
        free(line);
    }

    rl_clear_history();
#else
    }
#endif
}


int
main(int argc, char **argv)
{
    jml_vm_t *vm = jml_vm_new();
    bool success;

    if (vm == NULL)
        return EXIT_FAILURE;

    switch (argc) {
        case 1:
            jml_cli_repl(vm);
            printf("\n");
            success = true;
            break;

        case 2:
            success = jml_cli_run(vm, argv[1]);
            break;

        default:
            print_error("usage: %s [file]\n", argv[0]);
            success = false;
            break;
    }

    jml_vm_free(vm);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
