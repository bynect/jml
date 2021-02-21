#ifndef JML_COMPILER_H_
#define JML_COMPILER_H_

#include <jml.h>

#include <jml_type.h>
#include <jml_vm.h>
#include <jml_lexer.h>


typedef struct {
    jml_lexer_t                     lexer;
    jml_token_t                     current;
    jml_token_t                     previous;
    bool                            w_error;
    bool                            panicked;
} jml_parser_t;


typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} jml_parser_precedence;


typedef enum {
    FUNCTION_FN,
    FUNCTION_LAMBDA,
    FUNCTION_METHOD,
    FUNCTION_MAIN,
    FUNCTION_INIT
} jml_function_type;


typedef void (*jml_parser_fn)(bool assignable);


typedef struct {
    jml_parser_fn                   prefix;
    jml_parser_fn                   infix;
    jml_parser_precedence           precedence;
} jml_parser_rule;


typedef struct {
    int                             index;
    bool                            local;
} jml_upvalue_t;


typedef struct {
    jml_token_t                     name;
    int                             depth;
    bool                            captured;
} jml_local_t;


typedef struct jml_loop {
    struct jml_loop                *enclosing;
    int                             start;
    int                             body;
    int                             exit;
} jml_loop_t;


typedef struct jml_compiler {
    struct jml_compiler            *enclosing;
    jml_obj_function_t             *function;
    jml_function_type               type;
    jml_local_t                     locals[LOCAL_MAX];
    int                             local_count;
    jml_upvalue_t                   upvalues[LOCAL_MAX];
    int                             scope_depth;
    jml_obj_module_t               *module;
    jml_loop_t                     *loop;
    bool                            output;
} jml_compiler_t;


typedef struct jml_class_compiler {
    struct jml_class_compiler      *enclosing;
    jml_token_t                     name;
    bool                            w_superclass;
} jml_class_compiler_t;


void jml_compiler_mark_roots(void);

jml_obj_function_t *jml_compiler_compile(const char *source,
    jml_obj_module_t *module, bool output);


#endif /* JML_COMPILER_H_ */
