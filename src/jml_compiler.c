#include <string.h>

#include <jml_common.h>
#include <jml_compiler.h>
#include <jml_gc.h>



void
jml_compiler_mark_roots(void)
{
    jml_compiler_t compiler = current;
    while (compiler != NULL) {
        jml_gc_mark_obj((jml_obj_t*)compiler->function);
        compiler = compiler->enclosing;
    }
}
