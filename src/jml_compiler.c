#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <jml_common.h>
#include <jml_compiler.h>
#include <jml_type.h>
#include <jml_type.h>
#include <jml_gc.h>


jml_parser_t parser;

jml_compiler_t *current = NULL;

jml_class_compiler_t *class_current = NULL;

jml_bytecode_t *compiled;


static jml_bytecode_t *
jml_bytecode_current()
{
  return &(current->function->bytecode);
}


void
jml_compiler_mark_roots(void)
{
    jml_compiler_t compiler = current;
    while (compiler != NULL) {
        jml_gc_mark_obj((jml_obj_t*)compiler->function);
        compiler = compiler->enclosing;
    }
}


static void
jml_parser_error_at(jml_token_t *token,
    const char *message)
{
    if (parser.panicked) return;
    parser.panicked = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        /*pass*/
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.w_error = true;
}

static void
jml_parser_error(const char *message)
{
    jml_parser_error_at(&(parser.previous), message);
}

static void
jml_parser_error_current(const char *message)
{
    jml_parser_error_at(&parser.current, message);
}


static void
jml_parser_advance(void)
{
    parser.previous = parser.current;

    for ( ;; ) {
        parser.current = jml_lexer_tokenize();
        if (parser.current.type != TOKEN_ERROR) break;

        jml_parser_error_current(parser.current.start);
    }
}


static void
jml_parser_consume(jml_token_type type,
    const char *message)
{
    if (parser.current.type == type) {
        jml_parser_advance();
        return;
    }

    jml_parser_error_current(message);
}


static bool
jml_parser_check(jml_token_type type)
{
    return parser.current.type == type;
}


static bool
jml_parser_match(jml_token_type type)
{
    if (!jml_parser_check(type))
        return false;
    jml_parser_advance();
    return true;
}


static uint8_t
jml_bytecode_make_const(jml_value_t value)
{
    int constant = jml_bytecode_add_const(
        jml_bytecode_current(), value
    );

    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}


static void
jml_bytecode_emit_byte(uint8_t byte)
{
    jml_bytecode_write(
        jml_bytecode_current(), byte, parser.previous.line
    );
}


static void
jml_bytecode_emit_bytes(uint8_t byte1,
    uint8_t byte2)
{
    jml_bytecode_emit_byte(byte1);
    jml_bytecode_emit_byte(byte2);
}


static void
jml_bytecode_emit_loop(int start_pos)
{
    jml_bytecode_emit_byte(OP_LOOP);

    int offset = jml_bytecode_current()->count - start_pos + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    jml_bytecode_emit_byte((offset >> 8) & 0xff);
    jml_bytecode_emit_byte(offset & 0xff);
}


static int
jml_bytecode_emit_jump(uint8_t instruction)
{
    jml_bytecode_emit_byte(instruction);
    jml_bytecode_emit_byte(0xff);
    jml_bytecode_emit_byte(0xff);

    return jml_bytecode_current()->count - 2;
}


static void
jml_bytecode_patch_jump(int offset)
{
    int jump = jml_bytecode_current()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    jml_bytecode_current()->code[offset] = (jump >> 8) & 0xff;
    jml_bytecode_current()->code[offset + 1] = jump & 0xff;
}


static void
jml_bytecode_emit_return()
{
    if (current->type == FUNCTION_INITIALIZER) {
        jml_bytecode_emit_bytes(OP_GET_LOCAL, 0);
    } else {
        jml_bytecode_emit_byte(OP_NONE);
    }

    jml_bytecode_emit_byte(OP_RETURN);
}


static void
jml_bytecode_emit_constant(jml_value_t value)
{
    jml_bytecode_emit_bytes(
        OP_CONST, jml_bytecode_make_const(value)
    );
}


static void
jml_compiler_init(jml_compiler_t *compiler,
    jml_function_type type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;

    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = jml_obj_function_new();
    current = compiler;

    if (type != FUNCTION_MAIN) {
        current->function->name = jml_obj_string_copy(
            parser.previous.start, parser.previous.length
        );
    }

    jml_local_t *local = &(current->locals[current->local_count++]);
    local->depth = 0;

    if (type != FUNCTION_FN) {
        local->name.start = "self";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }

    local->captured = false;
}


static jml_obj_function_t *
jml_compiler_end()
{
    jml_bytecode_emit_return();
    jml_obj_function_t *function = current->function;

#ifdef JML_DISASSEMBLE
    if (!parser.w_error) {
        jml_bytecode_disassemble(jml_bytecode_current(),
        function->name != NULL ? function->name->chars : "<fn __main>");
    }
#endif

    current = current->enclosing;
    return function;
}


static void
jml_compiler_begin_scope()
{
    current->scope_depth++;
}


static void
jml_compiler_end_scope()
{
    current->scope_depth--;

    while (current->local_count > 0
        && current->locals[current->local_count - 1].depth >
            current->scope_depth) {

        if (current->locals[current->local_count - 1].captured) {
            jml_bytecode_emit_byte(OP_CLOSE_UPVALUE);
        } else {
            jml_bytecode_emit_byte(OP_POP);
        }

        current->local_count--;
    }
}
