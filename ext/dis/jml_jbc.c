#include <stdio.h>
#include <stdlib.h>

#include <jml.h>
#include <jml/jml_serialization.h>


bool
jml_dis_dump(const char *filename)
{
    jml_bytecode_t bytecode;

    if (!jml_deserialize_bytecode_file(&bytecode, filename)) {
        jml_bytecode_free(&bytecode);
        return false;
    }

    size_t size = 0;
    uint8_t *bytes = (uint8_t*)jml_file_read(filename, &size);

    if (bytes == NULL || size == 0) {
        jml_bytecode_free(&bytecode);
        return false;
    }

    printf(
        "%s (jml bytecode v%s)\n",
        filename,
        JML_VERSION_STRING
    );

    for (uint32_t i = 0, j = 0; i < size; ) {
        printf("%04x   |", i);
        for (j = 0; j < 25; ++j) {
            if (i + j < size) {
                printf("%c",
                    jml_is_printable(bytes[i + j]) ? bytes[i + j] : '.'
                );
            } else
#ifdef JML_DIS_PAD
                printf(" ");
#else
                break;
#endif
        }
        i += j;
        printf("|\n");
    }

    jml_realloc(bytes, 0);
    jml_bytecode_free(&bytecode);
    return true;
}


int
main(int argc, char **argv)
{
    jml_vm_t *vm = jml_vm_new();
    if (vm == NULL) return EXIT_FAILURE;

    switch (argc) {
        case 2: {
            jml_bytecode_t bytecode;
            if (!jml_deserialize_bytecode_file(&bytecode, argv[1])) {
                printf("invalid bytecode file %s\n", argv[1]);
                jml_bytecode_free(&bytecode);
                return EXIT_FAILURE;
            }

            jml_bytecode_disassemble(&bytecode, argv[1]);
            jml_bytecode_free(&bytecode);
            break;
        }

        case 3:
            if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--hex") == 0) {
                if (!jml_dis_dump(argv[2])) {
                    printf("invalid bytecode file %s\n", argv[1]);
                    return EXIT_FAILURE;
                }
                break;
            } /*fallthrough*/

        default:
            printf("usage: %s [file]\n", argv[0]);
            return EXIT_FAILURE;
    }

    jml_vm_free();
    return EXIT_SUCCESS;
}
