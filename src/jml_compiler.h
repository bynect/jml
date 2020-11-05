#ifndef _JML_COMPILER_H_
#define _JML_COMPILER_H_

#include <jml_common.h>
#include <jml_lexer.h>
#include <jml_type.h>


typedef struct {
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


typedef void (*jml_parser_fn)(bool assignable);


typedef struct {
    jml_parser_fn                   prefix;
    jml_parser_fn                   infix;
    jml_parser_precedence           precedence;
} jml_parser_rule;


typedef struct jml_compiler_t {
    struct jml_compiler_t          *enclosing;
    jml_obj_function_t             *function;
    jml_function_type               type;
    jml_local_t                     locals[LOCAL_MAX];
    uint8_t                         local_count;
    jml_upvalue_t                   upvalues[LOCAL_MAX];
    uint8_t                         scope_depth;
} jml_compiler_t;


typedef struct jml_class_compiler_t {
    struct jml_class_compiler_t    *enclosing;
    jml_token_t                     name;
    bool                            w_superclass;
} jml_class_compiler_t;


typedef struct {
    jml_token_t                     name;
    uint8_t                         depth;
    bool                            captured;
} jml_local_t;


typedef struct {
    uint8_t                         index;
    bool                            local;
} jml_upvalue_t;


typedef enum {
    FUNCTION_FN,
    FUNCTION_METHOD,
    FUNCTION_DUNDER,
    FUNCTION_MAIN,
    FUNCTION_INITIALIZER
} jml_function_type;


void jml_compiler_mark_roots(void);

jml_obj_function_t *jml_compiler_compile(
    const char *source);


#endif /* _JML_COMPILER_H_ */
