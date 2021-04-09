#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <jml.h>
#include <jml/jml_serialization.h>


#ifdef JML_CLI_READLINE

#include <readline/readline.h>
#include <readline/history.h>

#endif


#define print_error(message, ...)                       \
    do {                                                \
        fprintf(stderr, message, __VA_ARGS__);          \
        exit(EXIT_FAILURE);                             \
    } while (false)


static jml_vm_t *vm = NULL;


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
jml_cli_run(const char *path)
{
    char *buffer = jml_cli_fread(path);
    char *source = buffer;

    if (strlen(buffer) > 2 && buffer[0] == '#' && buffer[1] == '!') {
        while (*source != '\n' && *source != '\0')
            ++source;
    }

    jml_interpret_result result = jml_vm_interpret(vm, source);
    free(buffer);

    return result == INTERPRET_OK;
}


static void
jml_cli_repl(void)
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

        if (!IS_NONE(result)) {
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


static void
jml_cli_cleanup(void)
{
    jml_vm_free(vm);
}


int
main(int argc, char **argv)
{
    vm = jml_vm_new();
    if (vm == NULL) return EXIT_FAILURE;

    bool success;
    atexit(jml_cli_cleanup);

    switch (argc) {
        case 1:
            jml_cli_repl();
            printf("\n");
            success = true;
            break;

        case 2:
            success = jml_cli_run(argv[1]);
            break;

        case 3:
            if (strcmp(argv[1], "-b") == 0) {
                jml_bytecode_t bytecode;
                if (!jml_deserialize_bytecode_file(&bytecode, argv[2]))
                    print_error("invalid bytecode file %s\n", argv[2]);

                success = jml_vm_interpret_bytecode(vm, &bytecode) == INTERPRET_OK;
                break;
            } /*fallthrough*/

        default:
            print_error("usage: %s [-b] [file]\n", argv[0]);
            success = false;
            break;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
