#include <stdio.h>
#include <stdlib.h>

#include <jml.h>
#include <jml/jml_serialization.h>


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

        default:
            printf("usage: %s [file]\n", argv[0]);
            return EXIT_FAILURE;
    }

    jml_vm_free();
    return EXIT_SUCCESS;
}
