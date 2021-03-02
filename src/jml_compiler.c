#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jml_compiler.h>
#include <jml_vm.h>
#include <jml_gc.h>
#include <jml_util.h>
#include <jml_string.h>


#define EMIT_EXTENDED_OP1(compiler, o1, o2, b1)         \
    do {                                                \
        if (b1 > UINT8_MAX) {                           \
            jml_bytecode_emit_byte(compiler, o2);       \
            jml_bytecode_emit_bytes(compiler,           \
                (b1 >> 8) & 0xff, b1 & 0xff);           \
        } else                                          \
            jml_bytecode_emit_bytes(compiler, o1, b1);  \
    } while (false)


#define EMIT_EXTENDED_OP2(compiler, o1, o2, b1, b2)     \
    do {                                                \
        if (b1 > UINT8_MAX || b2 > UINT8_MAX) {         \
                                                        \
            jml_bytecode_emit_byte(compiler, o2);       \
            jml_bytecode_emit_bytes(compiler,           \
                (b1 >> 8) & 0xff, b1 & 0xff);           \
                                                        \
            jml_bytecode_emit_bytes(compiler,           \
                (b2 >> 8) & 0xff, b2 & 0xff);           \
        } else {                                        \
            jml_bytecode_emit_bytes(compiler, o1, b1);  \
            jml_bytecode_emit_byte(compiler, b2);       \
        }                                               \
    } while (false)


#define EMIT_EXTENDED_OP3(compiler, o1, o2, b1, b2, b3) \
    do {                                                \
        if (b1 > UINT8_MAX                              \
            || b2 > UINT8_MAX                           \
            || b3 > UINT8_MAX) {                        \
                                                        \
            jml_bytecode_emit_byte(compiler, o2);       \
            jml_bytecode_emit_bytes(compiler,           \
                (b1 >> 8) & 0xff, b1 & 0xff);           \
                                                        \
            jml_bytecode_emit_bytes(compiler,           \
                (b2 >> 8) & 0xff, b2 & 0xff);           \
                                                        \
            jml_bytecode_emit_bytes(compiler,           \
                (b3 >> 8) & 0xff, b3 & 0xff);           \
        } else {                                        \
            jml_bytecode_emit_bytes(compiler, o1, b1);  \
            jml_bytecode_emit_bytes(compiler, b2, b3);  \
        }                                               \
    } while (false)


static inline jml_bytecode_t *
jml_bytecode_current(jml_compiler_t *compiler)
{
    return &compiler->function->bytecode;
}


static jml_token_t
jml_token_emit_synthetic(jml_parser_t *parser,
    const char *text)
{
    jml_token_t token;
    token.start     = text;
    token.length    = strlen(text);
    token.line      = parser->current.line;
    token.offset    = 0;

    return token;
}


static void
jml_parser_error_at(jml_compiler_t *compiler,
    jml_token_t *token, const char *message)
{
    if (compiler->parser->panicked)
        return;

    if (compiler->output) {
        fprintf(stderr, "[line %d", token->line);

        if (compiler->module != NULL) {
            fprintf(stderr, " in module %s",
                compiler->module->name->chars);
        }

        fprintf(stderr, "] Error");

        if (token->type == TOKEN_EOF) {
            fprintf(stderr, " at eof");

        } else if (token->type == TOKEN_ERROR) {
            fprintf(stderr, " at '%c'",
                compiler->parser->lexer.source[token->offset]);

        } else if (strncmp(token->start, "\n", token->length) == 0) {
            fprintf(stderr, " at newline");

        } else {
            fprintf(stderr, " at '%.*s'",
                token->length, token->start);
        }

        fprintf(stderr, ": %s\n", message);
    }

    compiler->parser->panicked = true;
    compiler->parser->w_error  = true;
}


#define jml_parser_error(compiler, message)             \
    jml_parser_error_at(compiler, &compiler->parser->previous, message)


#define jml_parser_error_current(compiler, message)     \
    jml_parser_error_at(compiler, &compiler->parser->current, message)


static void
jml_parser_advance(jml_compiler_t *compiler)
{
    compiler->parser->previous = compiler->parser->current;

    while (true) {
        compiler->parser->current = jml_lexer_tokenize(&compiler->parser->lexer);

#ifdef JML_PRINT_TOKEN
        jml_token_type_print(compiler->parser->current.type);
        printf(
            "    %.*s\n",
            compiler->parser->current.length,
            compiler->parser->current.start
        );
#endif
        if (compiler->parser->current.type != TOKEN_ERROR)
            break;

        jml_parser_error_current(compiler, compiler->parser->current.start);
    }
}


static inline bool
jml_parser_check(jml_parser_t *parser, jml_token_type type)
{
    return parser->current.type == type;
}


static void
jml_parser_consume(jml_compiler_t *compiler,
    jml_token_type type, const char *message)
{
    if (jml_parser_check(compiler->parser, type)) {
        jml_parser_advance(compiler);
        return;
    }

    jml_parser_error_current(compiler, message);
}


static bool
jml_parser_match(jml_compiler_t *compiler, jml_token_type type)
{
    if (!jml_parser_check(compiler->parser, type))
        return false;

    jml_parser_advance(compiler);
    return true;
}


static bool
jml_parser_match_line(jml_compiler_t *compiler)
{
    if (!jml_parser_match(compiler, TOKEN_LINE)
        && !jml_parser_check(compiler->parser, TOKEN_RBRACE))
        return false;

    while (jml_parser_match(compiler, TOKEN_LINE));
    return true;
}


static void
jml_parser_newline(jml_compiler_t *compiler, const char *message)
{
    if (!jml_parser_check(compiler->parser, TOKEN_RBRACE)
        && !jml_parser_match(compiler, TOKEN_SEMI))
        jml_parser_consume(compiler, TOKEN_LINE, message);

    jml_parser_match_line(compiler);
}


static uint16_t
jml_bytecode_make_const(jml_compiler_t *compiler, jml_value_t value)
{
    int constant = jml_bytecode_add_const(
        jml_bytecode_current(compiler), value
    );

    if (constant > UINT8_MAX) {
        if (constant > UINT16_MAX) {
            jml_parser_error(
                compiler,
                "Too many constants in one chunk."
            );
            return 0;
        }

        return (uint16_t)constant;
    }

    return (uint8_t)constant;
}


static void
jml_bytecode_emit_byte(jml_compiler_t *compiler, uint8_t byte)
{
    jml_bytecode_write(
        jml_bytecode_current(compiler),
        byte,
        compiler->parser->previous.line
    );
}


static void
jml_bytecode_emit_bytes(jml_compiler_t *compiler,
    uint8_t byte1, uint8_t byte2)
{
    jml_bytecode_emit_byte(compiler, byte1);
    jml_bytecode_emit_byte(compiler, byte2);
}


static void
jml_bytecode_emit_loop(jml_compiler_t *compiler, int start)
{
    jml_bytecode_emit_byte(compiler, OP_LOOP);

    int offset = jml_bytecode_current(compiler)->count - start + 2;

    if (offset > UINT16_MAX)
        jml_parser_error(compiler, "Loop body too large.");

    jml_bytecode_emit_byte(compiler, (offset >> 8) & 0xff);
    jml_bytecode_emit_byte(compiler, offset & 0xff);
}


static int
jml_bytecode_emit_jump(jml_compiler_t *compiler, uint8_t instruction)
{
    jml_bytecode_emit_byte(compiler, instruction);
    jml_bytecode_emit_byte(compiler, 0xff);
    jml_bytecode_emit_byte(compiler, 0xff);

    return jml_bytecode_current(compiler)->count - 2;
}


static void
jml_bytecode_patch_jump(jml_compiler_t *compiler, int offset)
{
    int jump = jml_bytecode_current(compiler)->count - offset - 2;

    if (jump > UINT16_MAX) {
        jml_parser_error(compiler, "Too much code to jump over.");
    }

    jml_bytecode_current(compiler)->code[offset] = (jump >> 8) & 0xff;
    jml_bytecode_current(compiler)->code[offset + 1] = jump & 0xff;
}


static void
jml_bytecode_emit_return(jml_compiler_t *compiler)
{
    if (compiler->type == FUNCTION_INIT)
        jml_bytecode_emit_bytes(compiler, OP_GET_LOCAL, 0);

    else if (compiler->type != FUNCTION_MAIN
        && compiler->function->bytecode.count > 0) {
        for (int i = jml_bytecode_current(compiler)->count - 1; i >= 0; --i) {

            if (jml_bytecode_current(compiler)->code[i] == OP_POP) {
                jml_bytecode_current(compiler)->code[i] = OP_NOP;
                jml_bytecode_emit_byte(compiler, OP_RETURN);
                return;

            } else if (jml_bytecode_current(compiler)->code[i] == OP_POP_TWO) {
                jml_bytecode_current(compiler)->code[i] = OP_POP;
                jml_bytecode_emit_byte(compiler, OP_RETURN);
                return;
            }
        }

        jml_bytecode_emit_byte(compiler, OP_NONE);

    } else
        jml_bytecode_emit_byte(compiler, OP_NONE);

    jml_bytecode_emit_byte(compiler, OP_RETURN);
}


static void
jml_bytecode_emit_const(jml_compiler_t *compiler, jml_value_t value)
{
    uint16_t constant = jml_bytecode_make_const(compiler, value);
    EMIT_EXTENDED_OP1(
        compiler, OP_CONST, EXTENDED_OP(OP_CONST), constant
    );
}


static void
jml_compiler_init(jml_compiler_t *compiler, jml_compiler_t *enclosing,
    jml_parser_t *parser, jml_function_type type, jml_obj_module_t *module, bool output)
{
    if (type == FUNCTION_MAIN)
        ++vm->compiler_top;

    vm->compiler_top[-1]    = compiler;

    compiler->enclosing     = enclosing;
    compiler->function      = NULL;
    compiler->type          = type;

    compiler->module        = module;
    compiler->parser        = parser;
    compiler->klass         = enclosing != NULL ? enclosing->klass : NULL;

    compiler->local_count   = 0;
    compiler->scope_depth   = 0;
    compiler->function      = jml_obj_function_new();

    if (type == FUNCTION_LAMBDA)
        compiler->function->name = jml_obj_string_copy("lambda", 6);

    else if (type != FUNCTION_MAIN) {
        compiler->function->name = jml_obj_string_copy(
            parser->previous.start, parser->previous.length
        );

    } else if (module != NULL) {
        char name[128];
        int length = snprintf(name, 128, "%s.__main", module->name->chars);

        compiler->function->name = jml_obj_string_copy(name, length);
    }

    if (type == FUNCTION_METHOD || type == FUNCTION_INIT) {
        compiler->function->klass_name = jml_obj_string_copy(
            compiler->klass->name.start, compiler->klass->name.length
        );
    }

    jml_local_t *local      = &compiler->locals[compiler->local_count++];
    local->depth            = 0;
    local->captured         = false;

    if (type == FUNCTION_METHOD || type == FUNCTION_INIT) {
        local->name.start   = "self";
        local->name.length  = 4;

    } else {
        local->name.start   = "";
        local->name.length  = 0;
    }

    compiler->loop          = NULL;
    compiler->output        = output;
}


static jml_obj_function_t *
jml_compiler_end(jml_compiler_t *compiler)
{
    jml_bytecode_emit_return(compiler);
    jml_bytecode_emit_byte(compiler, OP_END);

    jml_obj_function_t *function = compiler->function;

#ifdef JML_DISASSEMBLE
    if (!compiler->parser->w_error) {
        jml_bytecode_disassemble(jml_bytecode_current(compiler),
            function->name != NULL ? function->name->chars : "__main");
    }
#endif

    vm->compiler_top[-1]         = compiler->enclosing;

    if (compiler->type == FUNCTION_MAIN)
        --vm->compiler_top;

    return function;
}


static inline void
jml_scope_begin(jml_compiler_t *compiler)
{
    ++compiler->scope_depth;
}


static inline void
jml_scope_end(jml_compiler_t *compiler)
{
    --compiler->scope_depth;

    while (compiler->local_count > 0
        && compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth) {

        if (compiler->locals[compiler->local_count - 1].captured)
            jml_bytecode_emit_byte(compiler, OP_CLOSE_UPVALUE);

        else
            jml_bytecode_emit_byte(compiler, OP_POP);

        compiler->local_count--;
    }
}


static inline uint16_t
jml_identifier_const(jml_compiler_t *compiler, jml_token_t *name)
{
    return jml_bytecode_make_const(
        compiler,
        OBJ_VAL(jml_obj_string_copy(name->start, name->length))
    );
}


static bool
jml_identifier_equal(jml_token_t *a, jml_token_t *b)
{
    if (a->length != b->length)
        return false;

    return memcmp(a->start, b->start, a->length) == 0;
}


static inline void
jml_local_mark(jml_compiler_t *compiler)
{
    compiler->locals[compiler->local_count - 1].depth = compiler->scope_depth;
}


static inline void
jml_local_unmark(jml_compiler_t *compiler)
{
    compiler->locals[compiler->local_count - 1].depth = -1;
}


static void
jml_local_add(jml_compiler_t *compiler, jml_token_t name)
{
    if (compiler->local_count == LOCAL_MAX) {
        jml_parser_error(
            compiler,
            "Too many local variables in function."
        );
        return;
    }

    jml_local_t *local  = &compiler->locals[compiler->local_count++];
    local->name         = name;
    local->depth        = -1;
    local->captured     = false;
}


static int
jml_local_resolve(jml_compiler_t *compiler, jml_token_t *name)
{
    for (int i = compiler->local_count - 1; i >= 0; --i) {
        jml_local_t *local = &compiler->locals[i];

        if (jml_identifier_equal(name, &local->name)) {
            if (local->depth == -1) {
                jml_parser_error(
                    compiler,
                    "Can't read local variable in its own initializer."
                );
            }
            return i;
        }
    }

    return -1;
}


static int
jml_local_add_synthetic(jml_compiler_t *compiler,
    jml_token_t *name)
{
    jml_local_add(compiler, *name);
    jml_local_mark(compiler);

    return jml_local_resolve(compiler, name);
}


static int
jml_upvalue_add(jml_compiler_t *compiler,
    uint8_t index, bool local)
{
    int upvalue_count = compiler->function->upvalue_count;

    for (int i = 0; i < upvalue_count; ++i) {
        jml_upvalue_t *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->local == local)
            return i;
    }

    if (upvalue_count == LOCAL_MAX) {
        jml_parser_error(
            compiler,
            "Too many upvalues in function."
        );
        return 0;
    }

    compiler->upvalues[upvalue_count].local = local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->function->upvalue_count++;
}


static int
jml_upvalue_resolve(jml_compiler_t *compiler,
    jml_token_t *name)
{
    if (compiler->enclosing == NULL)
        return -1;

    int local = jml_local_resolve(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].captured = true;
        return jml_upvalue_add(compiler, (uint8_t)local, true);
    }

    int upvalue = jml_upvalue_resolve(compiler->enclosing, name);
    if (upvalue != -1) {
        return jml_upvalue_add(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}


static void
jml_loop_begin(jml_compiler_t *compiler, jml_loop_t *loop,
    int start, int body, int exit)
{
    loop->start         = start;
    loop->body          = body;
    loop->exit          = exit;

    loop->enclosing     = compiler->loop;
    compiler->loop      = loop;
}


static void
jml_loop_end(jml_compiler_t *compiler)
{
    jml_loop_t *loop = compiler->loop;

    if (loop != NULL) {
        int count = jml_bytecode_current(compiler)->count;

        for (int i = loop->body; i < count; ) {
            if (jml_bytecode_current(compiler)->code[i] == UINT8_MAX) {
                jml_bytecode_current(compiler)->code[i] = OP_JUMP;
                jml_bytecode_patch_jump(compiler, i + 1);
                i += 3;
            } else
                ++i;
        }

        compiler->loop   = loop->enclosing;
    }
}


static void
jml_variable_declaration(jml_compiler_t *compiler)
{
    if (compiler->scope_depth == 0)
        return;

    jml_token_t *name = &compiler->parser->previous;

    for (int i = compiler->local_count - 1; i >= 0; --i) {
        jml_local_t *local = &compiler->locals[i];

        if (local->depth != -1
            && local->depth < compiler->scope_depth)
            break;

        if (jml_identifier_equal(name, &local->name)) {
            jml_parser_error(
                compiler,
                "Variable alredy declared in this scope."
            );
        }
    }

    jml_local_add(compiler, *name);
}


static uint16_t
jml_variable_parse(jml_compiler_t *compiler, const char *message)
{
    jml_parser_consume(compiler, TOKEN_NAME, message);

    jml_variable_declaration(compiler);
    if (compiler->scope_depth > 0)
        return 0;

    return jml_identifier_const(
        compiler, &compiler->parser->previous);
}


static void
jml_variable_definition(jml_compiler_t *compiler, uint16_t global)
{
    if (compiler->scope_depth > 0) {
        jml_local_mark(compiler);
        return;
    }

    EMIT_EXTENDED_OP1(
        compiler, OP_DEF_GLOBAL, EXTENDED_OP(OP_DEF_GLOBAL), global
    );
}


/*forwarded declaration*/
static void jml_expression(jml_compiler_t *compiler);

static void jml_statement(jml_compiler_t *compiler);

static void jml_declaration(jml_compiler_t *compiler);

static jml_parser_rule *jml_parser_rule_get(jml_token_type type);

static void jml_parser_precedence_parse(jml_compiler_t *compiler, jml_parser_precedence precedence);

static void jml_block(jml_compiler_t *compiler);


static void
jml_parser_synchronize(jml_compiler_t *compiler)
{
    compiler->parser->panicked = false;

    while (compiler->parser->current.type != TOKEN_EOF) {
        if (compiler->parser->previous.type == TOKEN_LINE) {
            jml_parser_match_line(compiler);
            return;
        }

        switch (compiler->parser->current.type) {
            case TOKEN_FOR:
            case TOKEN_WHILE:
            case TOKEN_BREAK:
            case TOKEN_SKIP:
            case TOKEN_MATCH:
            case TOKEN_WITH:
            case TOKEN_IF:
            case TOKEN_CLASS:
            case TOKEN_LET:
            case TOKEN_FN:
            case TOKEN_RETURN:
            case TOKEN_IMPORT:
                return;

            default:
                break;
        }

        jml_parser_advance(compiler);
    }
}


static uint8_t
jml_arguments_list(jml_compiler_t *compiler)
{
    jml_parser_match_line(compiler);

    uint8_t arg_count = 0;
    if (!jml_parser_check(compiler->parser, TOKEN_RPAREN)) {
        do {
            jml_parser_match_line(compiler);

            jml_expression(compiler);

            if (arg_count == 255)
                jml_parser_error(
                    compiler,
                    "Can't have more than 255 arguments."
                );

            ++arg_count;
        } while (jml_parser_match(compiler, TOKEN_COMMA));
    }

    jml_parser_match_line(compiler);
    jml_parser_consume(compiler, TOKEN_RPAREN, "Expect ')' after arguments.");
    return arg_count;
}


static void
jml_and(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);

    int jump_end = jml_bytecode_emit_jump(compiler, OP_JUMP_IF_FALSE);
    jml_bytecode_emit_byte(compiler, OP_POP);

    jml_parser_precedence_parse(compiler, PREC_AND);
    jml_bytecode_patch_jump(compiler, jump_end);

    jml_bytecode_emit_byte(compiler, OP_BOOL);
}


static void
jml_or(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);

    int jump_else = jml_bytecode_emit_jump(compiler, OP_JUMP_IF_FALSE);
    int jump_end  = jml_bytecode_emit_jump(compiler, OP_JUMP);

    jml_bytecode_patch_jump(compiler, jump_else);
    jml_bytecode_emit_byte(compiler, OP_POP);

    jml_parser_precedence_parse(compiler, PREC_OR);
    jml_bytecode_patch_jump(compiler, jump_end);

    jml_bytecode_emit_byte(compiler, OP_BOOL);
}


static void
jml_binary(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);

    jml_token_type type = compiler->parser->previous.type;

    jml_parser_rule *rule = jml_parser_rule_get(type);
    jml_parser_precedence_parse(
        compiler,
        (jml_parser_precedence)(rule->precedence + 1)
    );

    switch (type) {
        case TOKEN_COLCOLON:    jml_bytecode_emit_byte(compiler, OP_CONCAT);    break;
        case TOKEN_PLUS:        jml_bytecode_emit_byte(compiler, OP_ADD);       break;
        case TOKEN_MINUS:       jml_bytecode_emit_byte(compiler, OP_SUB);       break;
        case TOKEN_STAR:        jml_bytecode_emit_byte(compiler, OP_MUL);       break;
        case TOKEN_STARSTAR:    jml_bytecode_emit_byte(compiler, OP_POW);       break;
        case TOKEN_SLASH:       jml_bytecode_emit_byte(compiler, OP_DIV);       break;
        case TOKEN_PERCENT:     jml_bytecode_emit_byte(compiler, OP_MOD);       break;

        case TOKEN_EQEQUAL:     jml_bytecode_emit_byte(compiler, OP_EQUAL);     break;
        case TOKEN_GREATER:     jml_bytecode_emit_byte(compiler, OP_GREATER);   break;
        case TOKEN_GREATEREQ:   jml_bytecode_emit_byte(compiler, OP_GREATEREQ); break;
        case TOKEN_LESS:        jml_bytecode_emit_byte(compiler, OP_LESS);      break;
        case TOKEN_LESSEQ:      jml_bytecode_emit_byte(compiler, OP_LESSEQ);    break;
        case TOKEN_NOTEQ:       jml_bytecode_emit_byte(compiler, OP_NOTEQ);     break;

        case TOKEN_IN:          jml_bytecode_emit_byte(compiler, OP_CONTAIN);   break;

        default:                JML_UNREACHABLE();
    }
}


static void
jml_unary(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_token_type type = compiler->parser->previous.type;
    jml_parser_precedence_parse(compiler, PREC_UNARY);

    switch (type) {
        case TOKEN_NOT:         jml_bytecode_emit_byte(compiler, OP_NOT);       break;
        case TOKEN_MINUS:       jml_bytecode_emit_byte(compiler, OP_NEG);       break;

        default:                JML_UNREACHABLE();
    }
}


static void
jml_call(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    uint8_t arg_count = jml_arguments_list(compiler);
    jml_bytecode_emit_bytes(compiler, OP_CALL, arg_count);
}


static void
jml_dot(jml_compiler_t *compiler, bool assignable)
{
    jml_parser_consume(compiler, TOKEN_NAME, "Expect identifier after '.'.");
    uint16_t name = jml_identifier_const(compiler, &compiler->parser->previous);

    if (assignable
        && jml_parser_match(compiler, TOKEN_EQUAL)) {

        jml_parser_match_line(compiler);
        jml_expression(compiler);
        EMIT_EXTENDED_OP1(
            compiler, OP_SET_MEMBER, EXTENDED_OP(OP_SET_MEMBER), name
        );

    } else if (jml_parser_match(compiler, TOKEN_LPAREN)) {
        uint16_t arg_count = jml_arguments_list(compiler);
        EMIT_EXTENDED_OP2(
            compiler, OP_INVOKE, EXTENDED_OP(OP_INVOKE), name, arg_count
        );

    } else
        EMIT_EXTENDED_OP1(
            compiler, OP_GET_MEMBER, EXTENDED_OP(OP_GET_MEMBER), name
        );
}


static void
jml_grouping(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);
    jml_expression(compiler);

    jml_parser_match_line(compiler);
    jml_parser_consume(compiler, TOKEN_RPAREN, "Expect ')' after expression.");
}


static void
jml_indexing(jml_compiler_t *compiler, bool assignable)
{
    jml_parser_match_line(compiler);
    jml_expression(compiler);

    jml_parser_match_line(compiler);
    jml_parser_consume(compiler, TOKEN_RSQARE, "Expect ']' after indexing.");

    if (assignable
        && jml_parser_match(compiler, TOKEN_EQUAL)) {

        jml_parser_match_line(compiler);
        jml_expression(compiler);
        jml_bytecode_emit_byte(compiler, OP_SET_INDEX);

    } else
        jml_bytecode_emit_byte(compiler, OP_GET_INDEX);
}


static void
jml_literal(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    switch (compiler->parser->previous.type) {
        case TOKEN_FALSE:       jml_bytecode_emit_byte(compiler, OP_FALSE);     break;
        case TOKEN_NONE:        jml_bytecode_emit_byte(compiler, OP_NONE);      break;
        case TOKEN_TRUE:        jml_bytecode_emit_byte(compiler, OP_TRUE);      break;

        default:                JML_UNREACHABLE();
    }
}


static void
jml_number(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    double value = strtod(compiler->parser->previous.start, NULL);
    jml_bytecode_emit_const(compiler, NUM_VAL(value));
}


static void
jml_string(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    const char *raw             = compiler->parser->previous.start + 1;
    size_t length               = compiler->parser->previous.length - 2;

    char *buffer                = ALLOCATE(char, length + 1);
    size_t size                 = 0;

    for (size_t i = 0; i < length; ) {
        uint32_t c = raw[i];

        if (c == '\\') {
            if (i + 1 >= length) {
                jml_parser_error_current(
                    compiler,
                    "Invalid string escape sequence."
                );
                break;
            }

            switch (raw[i + 1]) {
                case '\'':              c = '\047'; ++i;                        break;
                case  '"':              c = '\042'; ++i;                        break;
                case '\\':              c = '\134'; ++i;                        break;
                case  'n':              c = '\012'; ++i;                        break;
                case  'r':              c = '\015'; ++i;                        break;
                case  't':              c = '\011'; ++i;                        break;
                case  'e':              c = '\033'; ++i;                        break;

                case  'x': {
                    if (i + 3 >= length) {
                        jml_parser_error_current(
                            compiler,
                            "Invalid string escape sequence."
                        );
                        break;
                    }

                    if (!jml_is_hex(raw[i + 2]) || !jml_is_hex(raw[i + 3])) {
                        jml_parser_error_current(
                            compiler,
                            "Invalid string escape sequence."
                        );
                        break;
                    }

                    char hex[3] = {
                        raw[i + 2],
                        raw[i + 3],
                        '\0'
                    };

                    c = strtoul(hex, NULL, 16);
                    if (c > 0)
                        buffer[size++] = c;
                    else {
                        buffer[size++] = '\\';
                        buffer[size++] = '0';
                    }

                    i += 4;
                    continue;
                }

                case  'u':  {
                    if (i + 5 >= length) {
                        jml_parser_error_current(
                            compiler,
                            "Invalid string escape sequence."
                        );
                        break;
                    }

                    if (!jml_is_hex(raw[i + 2]) || !jml_is_hex(raw[i + 3])
                        || !jml_is_hex(raw[i + 4]) || !jml_is_hex(raw[i + 5])) {
                        jml_parser_error_current(
                            compiler,
                            "Invalid string escape sequence."
                        );
                        break;
                    }

                    char hex[5] = {
                        raw[i + 2],
                        raw[i + 3],
                        raw[i + 4],
                        raw[i + 5],
                        '\0'
                    };

                    uint32_t value = strtoul(hex, NULL, 16);
                    size += jml_string_encode(&buffer[size], value);
                    i += 6;
                    continue;
                }

                case  'U':  {
                    if (i + 9 >= length) {
                        jml_parser_error_current(
                            compiler,
                            "Invalid string escape sequence."
                        );
                        break;
                    }

                    if (!jml_is_hex(raw[i + 2]) || !jml_is_hex(raw[i + 3])
                        || !jml_is_hex(raw[i + 4]) || !jml_is_hex(raw[i + 5])
                        || !jml_is_hex(raw[i + 6]) || !jml_is_hex(raw[i + 7])
                        || !jml_is_hex(raw[i + 8]) || !jml_is_hex(raw[i + 9])) {

                        jml_parser_error_current(
                            compiler,
                            "Invalid string escape sequence."
                        );
                        break;
                    }

                    char hex[9] = {
                        raw[i + 2],
                        raw[i + 3],
                        raw[i + 4],
                        raw[i + 5],
                        raw[i + 6],
                        raw[i + 7],
                        raw[i + 8],
                        raw[i + 9],
                        '\0'
                    };

                    uint32_t value = strtoul(hex, NULL, 16);
                    size += jml_string_encode(&buffer[size], value);
                    i += 10;
                    continue;
                }
            }
        }

        buffer[size] = c;
        ++size, ++i;
    }

    buffer[size] = '\0';

    jml_bytecode_emit_const(
        compiler,
        OBJ_VAL(jml_obj_string_take(buffer, size))
    );
}


static void
jml_array(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);

    uint16_t item_count = 0;
    if (!jml_parser_check(compiler->parser, TOKEN_RSQARE)) {
        do {
            jml_parser_match_line(compiler);
            jml_expression(compiler);

            if (item_count == 65535) {
                jml_parser_error(
                    compiler,
                    "Can't have more than 65535 items in an array."
                );
            }

            jml_parser_match_line(compiler);
            ++item_count;
        } while (jml_parser_match(compiler, TOKEN_COMMA));
    }

    jml_parser_match_line(compiler);
    jml_parser_consume(compiler, TOKEN_RSQARE, "Expect ']' after array.");

    EMIT_EXTENDED_OP1(
        compiler, OP_ARRAY, OP_ARRAY_EXTENDED, item_count
    );
}


static void
jml_map(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);

    uint16_t item_count = 0;
    if (!jml_parser_check(compiler->parser, TOKEN_RBRACE)) {
        do {
            jml_parser_match_line(compiler);
            jml_parser_advance(compiler);

            jml_string(compiler, false);
            jml_parser_match_line(compiler);

            jml_parser_consume(compiler, TOKEN_COLON, "Expect colon in map.");

            jml_parser_match_line(compiler);
            jml_expression(compiler);

            if (item_count == 65534) {
                jml_parser_error(
                    compiler,
                    "Can't have more than 65534 items in map."
                );
            }

            jml_parser_match_line(compiler);
            item_count += 2;
        } while (jml_parser_match(compiler, TOKEN_COMMA));
    }

    jml_parser_match_line(compiler);
    jml_parser_consume(compiler, TOKEN_RBRACE, "Expect '}' after map.");

    EMIT_EXTENDED_OP1(
        compiler, OP_MAP, OP_MAP_EXTENDED, item_count
    );
}


static void
jml_variable_assignment(jml_compiler_t *compiler, bool index,
    uint8_t get_op, uint8_t set_op, uint16_t arg, uint8_t op)
{
    if (index) {
        jml_bytecode_emit_byte(compiler, OP_SAVE);
        EMIT_EXTENDED_OP1(compiler, get_op, get_op, arg);
        jml_bytecode_emit_bytes(compiler, OP_ROT, OP_GET_INDEX);

        jml_parser_match_line(compiler);
        jml_expression(compiler);

        jml_bytecode_emit_bytes(compiler, op, OP_SET_INDEX);

    } else {
        EMIT_EXTENDED_OP1(compiler, get_op, get_op, arg);
        jml_parser_match_line(compiler);
        jml_expression(compiler);

        jml_bytecode_emit_byte(compiler, op);
        EMIT_EXTENDED_OP1(compiler, set_op, set_op, arg);
    }
}


static void
jml_variable_named(jml_compiler_t *compiler,
    jml_token_t name, bool assignable)
{
    uint8_t get_op, set_op;
    bool index  = false;
    int  arg    = jml_local_resolve(compiler, &name);

    if (arg != -1) {
        get_op  = OP_GET_LOCAL;
        set_op  = OP_SET_LOCAL;

    } else if ((arg = jml_upvalue_resolve(compiler, &name)) != -1) {
        get_op  = OP_GET_UPVALUE;
        set_op  = OP_SET_UPVALUE;

    } else {
        arg     = jml_identifier_const(compiler, &name);
        get_op  = arg > UINT8_MAX ? EXTENDED_OP(OP_GET_GLOBAL) : OP_GET_GLOBAL;
        set_op  = arg > UINT8_MAX ? EXTENDED_OP(OP_SET_GLOBAL) : OP_SET_GLOBAL;
    }

    if (jml_parser_match(compiler, TOKEN_LSQARE)) {
        EMIT_EXTENDED_OP1(compiler, get_op, get_op, arg);

        jml_parser_match_line(compiler);
        jml_expression(compiler);

        jml_parser_match_line(compiler);
        jml_parser_consume(compiler, TOKEN_RSQARE, "Expect ']' after indexing.");

        index = true;
    }

    if (assignable
        && jml_parser_match(compiler, TOKEN_EQUAL)) {

        jml_parser_match_line(compiler);
        jml_expression(compiler);

        if (index)
            jml_bytecode_emit_byte(compiler, OP_SET_INDEX);
        else
            EMIT_EXTENDED_OP1(compiler, set_op, set_op, arg);

    } else if (assignable
        && jml_parser_match(compiler, TOKEN_COLCOLONEQ)) {

        jml_variable_assignment(
            compiler, index, get_op, set_op, arg, OP_CONCAT
        );

    } else if (assignable
        && jml_parser_match(compiler, TOKEN_PLUSEQ)) {

        jml_variable_assignment(
            compiler, index, get_op, set_op, arg, OP_ADD
        );

    } else if (assignable
        && jml_parser_match(compiler, TOKEN_MINUSEQ)) {

        jml_variable_assignment(
            compiler, index, get_op, set_op, arg, OP_SUB
        );

    } else if (assignable
        && jml_parser_match(compiler, TOKEN_STAREQ)) {

        jml_variable_assignment(
            compiler, index, get_op, set_op, arg, OP_MUL
        );

    } else if (assignable
        && jml_parser_match(compiler, TOKEN_STARSTAREQ)) {

        jml_variable_assignment(
            compiler, index, get_op, set_op, arg, OP_POW
        );

    } else if (assignable
        && jml_parser_match(compiler, TOKEN_SLASHEQ)) {

        jml_variable_assignment(
            compiler, index, get_op, set_op, arg, OP_DIV
        );

    } else if (assignable
        && jml_parser_match(compiler, TOKEN_PERCENTEQ)) {

        jml_variable_assignment(
            compiler, index, get_op, set_op, arg, OP_MOD
        );

    } else if (jml_parser_match(compiler, TOKEN_ARROW)) {
        if (!index) {
            jml_parser_match_line(compiler);
            jml_parser_consume(compiler, TOKEN_NAME, "Expect identifier after '->'.");

            jml_bytecode_emit_byte(compiler, OP_NONE);

            if (jml_identifier_equal(&name, &compiler->parser->previous))
                jml_parser_error(compiler, "Can't swap variable to itself.");

            if (get_op == OP_GET_GLOBAL) {
                uint16_t new_arg = jml_identifier_const(compiler, &compiler->parser->previous);
                EMIT_EXTENDED_OP2(
                    compiler, OP_SWAP_GLOBAL, EXTENDED_OP(OP_SWAP_GLOBAL), arg, new_arg
                );

            } else
                memcpy(&compiler->locals[arg].name, &compiler->parser->previous, sizeof(jml_token_t));
        } else
            jml_parser_error(compiler, "Can't swap indexed value.");

    } else if (index)
        jml_bytecode_emit_byte(compiler, OP_GET_INDEX);

    else {
        EMIT_EXTENDED_OP1(
            compiler, get_op, get_op, arg
        );
    }
}


static void
jml_variable(jml_compiler_t *compiler, bool assignable)
{
    jml_variable_named(
        compiler, compiler->parser->previous, assignable
    );
}


static void
jml_super(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);

    if (compiler->klass == NULL)
        jml_parser_error(compiler, "Can't use 'super' outside of a class.");

    else if (!compiler->klass->w_superclass)
        jml_parser_error(compiler, "Can't use 'super' in a class without superclass.");

    jml_parser_consume(compiler, TOKEN_DOT, "Expect '.' after 'super'.");
    jml_parser_consume(compiler, TOKEN_NAME, "Expect superclass method name.");

    uint16_t name = jml_identifier_const(compiler, &compiler->parser->previous);
    jml_variable_named(compiler,
        jml_token_emit_synthetic(compiler->parser, "self"), false);

    if (jml_parser_match(compiler, TOKEN_LPAREN)) {
        uint16_t arg_count = jml_arguments_list(compiler);
        jml_variable_named(compiler, jml_token_emit_synthetic(compiler->parser, "super"), false);

        EMIT_EXTENDED_OP2(
            compiler, OP_SUPER_INVOKE, EXTENDED_OP(OP_SUPER_INVOKE), name, arg_count
        );

    } else {
        jml_variable_named(compiler, jml_token_emit_synthetic(compiler->parser, "super"), false);
        EMIT_EXTENDED_OP1(
            compiler, OP_SUPER, EXTENDED_OP(OP_SUPER), name
        );
    }
}


static void
jml_self(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);

    if (compiler->klass == NULL) {
        jml_parser_error(
            compiler,
            "Can't use 'self' outside of a class."
        );
        return;
    }

    jml_variable(compiler, false);
}


static void
jml_lambda(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_compiler_t sub_compiler;
    jml_compiler_init(&sub_compiler, compiler, compiler->parser,
        FUNCTION_LAMBDA, compiler->module, compiler->output);

    jml_scope_begin(&sub_compiler);

    if (!jml_parser_check(sub_compiler.parser, TOKEN_VBAR)) {
        do {
            ++sub_compiler.function->arity;
            if (sub_compiler.function->arity > 255) {
                jml_parser_error_current(
                    (&sub_compiler),
                    "Can't have more than 255 parameters."
                );
            }

            uint16_t param_const = jml_variable_parse(&sub_compiler, "Expect parameter name.");
            jml_variable_definition(&sub_compiler, param_const);
        } while (jml_parser_match(&sub_compiler, TOKEN_COMMA));
    }

    jml_parser_consume(&sub_compiler, TOKEN_VBAR, "Expect '|' after parameters.");
    jml_parser_consume(&sub_compiler, TOKEN_LBRACE, "Expect '{' before lambda body.");
    jml_block(&sub_compiler);

    jml_obj_function_t *function = jml_compiler_end(&sub_compiler);
    uint16_t constant = jml_bytecode_make_const(compiler, OBJ_VAL(function));
    EMIT_EXTENDED_OP1(
        compiler, OP_CLOSURE, EXTENDED_OP(OP_CLOSURE), constant
    );

    for (int i = 0; i < function->upvalue_count; ++i) {
        jml_bytecode_emit_byte(compiler, sub_compiler.upvalues[i].local ? 1 : 0);
        jml_bytecode_emit_byte(compiler, sub_compiler.upvalues[i].index);
    }
}


static void
jml_piping(jml_compiler_t *compiler, JML_UNUSED(bool assignable))
{
    jml_parser_match_line(compiler);
    jml_parser_precedence_parse(compiler, PREC_PIPE);

    jml_bytecode_emit_byte(compiler, OP_ROT);
    uint8_t arg_count = 1;

    if (jml_parser_match(compiler, TOKEN_LPAREN))
        arg_count += jml_arguments_list(compiler);

    jml_bytecode_emit_bytes(compiler, OP_CALL, arg_count);
}


static jml_parser_rule rules[] = {
    /*TOKEN_RPAREN*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_LPAREN*/    {&jml_grouping, &jml_call,      PREC_CALL},
    /*TOKEN_RSQARE*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_LSQARE*/    {&jml_array,    &jml_indexing,  PREC_CALL},
    /*TOKEN_RBRACE*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_LBRACE*/    {&jml_map,      NULL,           PREC_NONE},
    /*TOKEN_SEMI*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_COMMA*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_DOT*/       {NULL,          &jml_dot,       PREC_CALL},

    /*TOKEN_USCORE*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_CARET*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_AMP*/       {NULL,          NULL,           PREC_NONE},
    /*TOKEN_TILDE*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_QUEST*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_BANG*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_HASH*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_AT*/        {NULL,          NULL,           PREC_NONE},
    /*TOKEN_ARROW*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_VBAR*/      {&jml_lambda,   NULL,           PREC_NONE},
    /*TOKEN_PIPE*/      {NULL,          &jml_piping,    PREC_PIPE},
    /*TOKEN_COLON*/     {NULL,          NULL,           PREC_NONE},

    /*TOKEN_COLCOLON*/  {NULL,          &jml_binary,    PREC_TERM},
    /*TOKEN_COLCOLONEQ*/{NULL,          NULL,           PREC_NONE},
    /*TOKEN_PLUS*/      {NULL,          &jml_binary,    PREC_TERM},
    /*TOKEN_PLUSEQ*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_MINUS*/     {&jml_unary,    &jml_binary,    PREC_TERM},
    /*TOKEN_MINUSEQ*/   {NULL,          NULL,           PREC_NONE},
    /*TOKEN_STAR*/      {NULL,          &jml_binary,    PREC_FACTOR},
    /*TOKEN_STAREQ*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_STARSTAR*/  {NULL,          &jml_binary,    PREC_FACTOR},
    /*TOKEN_STARSTAREQ*/{NULL,          NULL,           PREC_NONE},
    /*TOKEN_SLASH*/     {NULL,          &jml_binary,    PREC_FACTOR},
    /*TOKEN_SLASHEQ*/   {NULL,          NULL,           PREC_NONE},
    /*TOKEN_PERCENT*/   {NULL,          &jml_binary,    PREC_FACTOR},
    /*TOKEN_PERCENTEQ*/ {NULL,          NULL,           PREC_NONE},

    /*TOKEN_EQUAL*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_EQEQUAL*/   {NULL,          &jml_binary,    PREC_EQUALITY},
    /*TOKEN_GREATER*/   {NULL,          &jml_binary,    PREC_COMPARISON},
    /*TOKEN_GREATEREQ*/ {NULL,          &jml_binary,    PREC_COMPARISON},
    /*TOKEN_LESS*/      {NULL,          &jml_binary,    PREC_COMPARISON},
    /*TOKEN_LESSEQ*/    {NULL,          &jml_binary,    PREC_COMPARISON},
    /*TOKEN_NOTEQ*/     {NULL,          &jml_binary,    PREC_EQUALITY},

    /*TOKEN_FOR*/       {NULL,          NULL,           PREC_NONE},
    /*TOKEN_WHILE*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_BREAK*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_SKIP*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_IN*/        {NULL,          &jml_binary,    PREC_COMPARISON},
    /*TOKEN_MATCH*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_WITH*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_IF*/        {NULL,          NULL,           PREC_NONE},
    /*TOKEN_ELSE*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_CLASS*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_SELF*/      {&jml_self,     NULL,           PREC_NONE},
    /*TOKEN_SUPER*/     {&jml_super,    NULL,           PREC_NONE},
    /*TOKEN_LET*/       {NULL,          NULL,           PREC_NONE},
    /*TOKEN_FN*/        {NULL,          NULL,           PREC_NONE},
    /*TOKEN_RETURN*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_IMPORT*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_ASYNC*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_AWAIT*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_AND*/       {NULL,          &jml_and,       PREC_AND},
    /*TOKEN_NOT*/       {&jml_unary,    NULL,           PREC_NONE},
    /*TOKEN_OR*/        {NULL,          &jml_or,        PREC_OR},

    /*TOKEN_TRUE*/      {&jml_literal,  NULL,           PREC_NONE},
    /*TOKEN_FALSE*/     {&jml_literal,  NULL,           PREC_NONE},
    /*TOKEN_NONE*/      {&jml_literal,  NULL,           PREC_NONE},
    /*TOKEN_NAME*/      {&jml_variable, NULL,           PREC_NONE},
    /*TOKEN_NUMBER*/    {&jml_number,   NULL,           PREC_NONE},
    /*TOKEN_STRING*/    {&jml_string,   NULL,           PREC_NONE},

    /*TOKEN_LINE*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_ERROR*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_EOF*/       {NULL,          NULL,           PREC_NONE}
};


static jml_parser_rule *
jml_parser_rule_get(jml_token_type type)
{
    JML_ASSERT(
        type >= TOKEN_RPAREN && type <= TOKEN_EOF,
        "Invalid token type %d.",
        (int)type
    );

    return &rules[type];
}


static void
jml_parser_precedence_parse(jml_compiler_t *compiler,
    jml_parser_precedence precedence)
{
    jml_parser_advance(compiler);

    jml_parser_fn prefix_rule = jml_parser_rule_get(
        compiler->parser->previous.type)->prefix;

    if (prefix_rule == NULL) {
        jml_parser_error(compiler, "Expect expression.");
        return;
    }

    bool assignable = precedence <= PREC_ASSIGNMENT;
    prefix_rule(compiler, assignable);

    while (precedence <= jml_parser_rule_get(
        compiler->parser->current.type)->precedence) {

        jml_parser_advance(compiler);
        jml_parser_fn infix_rule = jml_parser_rule_get(
            compiler->parser->previous.type)->infix;

        infix_rule(compiler, assignable);
    }

    if (assignable
        && (jml_parser_match(compiler, TOKEN_EQUAL)
        || jml_parser_match(compiler, TOKEN_COLCOLONEQ)
        || jml_parser_match(compiler, TOKEN_PLUSEQ)
        || jml_parser_match(compiler, TOKEN_MINUSEQ)
        || jml_parser_match(compiler, TOKEN_STAREQ)
        || jml_parser_match(compiler, TOKEN_STARSTAREQ)
        || jml_parser_match(compiler, TOKEN_SLASHEQ)
        || jml_parser_match(compiler, TOKEN_PERCENTEQ))) {

        jml_parser_error(compiler, "Invalid assignment target.");
    }
}


static void
jml_expression(jml_compiler_t *compiler)
{
    jml_parser_precedence_parse(compiler, PREC_ASSIGNMENT);
}


static void
jml_block(jml_compiler_t *compiler)
{
    jml_parser_match_line(compiler);

    while (!jml_parser_check(compiler->parser, TOKEN_RBRACE)
        && !jml_parser_check(compiler->parser, TOKEN_EOF))
        jml_declaration(compiler);

    jml_parser_consume(compiler, TOKEN_RBRACE, "Expect '}' after block.");
}


static void
jml_function(jml_compiler_t *compiler, jml_function_type type)
{
    jml_compiler_t sub_compiler;
    jml_compiler_init(&sub_compiler, compiler, compiler->parser,
        type, compiler->module, compiler->output);

    jml_scope_begin(&sub_compiler);

    jml_parser_consume(&sub_compiler, TOKEN_LPAREN, "Expect '(' after function name.");

    if (!jml_parser_check(sub_compiler.parser, TOKEN_RPAREN)) {
        do {
            ++sub_compiler.function->arity;
            if (sub_compiler.function->arity > 255) {
                jml_parser_error_current(
                    (&sub_compiler),
                    "Can't have more than 255 parameters."
                );
            }

            uint16_t param_const = jml_variable_parse(&sub_compiler, "Expect parameter name.");
            jml_variable_definition(&sub_compiler, param_const);
        } while (jml_parser_match(&sub_compiler, TOKEN_COMMA));
    }

    jml_parser_consume(&sub_compiler, TOKEN_RPAREN, "Expect ')' after parameters.");
    jml_parser_consume(&sub_compiler, TOKEN_LBRACE, "Expect '{' before function body.");

    jml_block(&sub_compiler);
    jml_parser_newline(&sub_compiler, "Expect newline after 'fn' declaration.");

    jml_obj_function_t *function = jml_compiler_end(&sub_compiler);
    uint16_t constant = jml_bytecode_make_const(compiler, OBJ_VAL(function));
    EMIT_EXTENDED_OP1(
        compiler, OP_CLOSURE, EXTENDED_OP(OP_CLOSURE), constant
    );

    for (int i = 0; i < function->upvalue_count; ++i) {
        jml_bytecode_emit_byte(compiler, sub_compiler.upvalues[i].local ? 1 : 0);
        jml_bytecode_emit_byte(compiler, sub_compiler.upvalues[i].index);
    }
}


static void
jml_method(jml_compiler_t *compiler)
{
    jml_parser_match_line(compiler);

    jml_parser_consume(compiler, TOKEN_NAME, "Expect method name.");
    uint16_t constant = jml_identifier_const(compiler, &compiler->parser->previous);

    jml_function_type type = FUNCTION_METHOD;

    if (compiler->parser->previous.length == 6 &&
        memcmp(compiler->parser->previous.start, "__init", 6) == 0)
        type = FUNCTION_INIT;

    jml_function(compiler, type);
    EMIT_EXTENDED_OP1(
        compiler, OP_METHOD, EXTENDED_OP(OP_METHOD), constant
    );
}


static void
jml_class_declaration(jml_compiler_t *compiler)
{
    jml_parser_match_line(compiler);

    jml_parser_consume(compiler, TOKEN_NAME, "Expect class name.");
    jml_token_t class_name      = compiler->parser->previous;

    uint16_t name_const         = jml_identifier_const(
        compiler, &compiler->parser->previous);
    jml_variable_declaration(compiler);

    EMIT_EXTENDED_OP1(
        compiler, OP_CLASS, EXTENDED_OP(OP_CLASS), name_const
    );
    jml_variable_definition(compiler, name_const);

    jml_class_compiler_t class_compiler;
    class_compiler.name         = compiler->parser->previous;
    class_compiler.enclosing    = compiler->klass;
    class_compiler.w_superclass = false;
    compiler->klass             = &class_compiler;

    if (jml_parser_match(compiler, TOKEN_LESS)) {
        jml_parser_match_line(compiler);

        jml_parser_consume(compiler, TOKEN_NAME, "Expect superclass name.");
        jml_variable(compiler, false);

        while (jml_parser_match(compiler, TOKEN_DOT)) {
            jml_parser_consume(compiler, TOKEN_NAME, "Expect identifier after '.'.");
            uint16_t name       = jml_identifier_const(
                compiler, &compiler->parser->previous);

            EMIT_EXTENDED_OP1(
                compiler, OP_GET_MEMBER, EXTENDED_OP(OP_GET_MEMBER), name
            );
        }

        if (jml_identifier_equal(&class_name, &compiler->parser->previous)) {
            jml_parser_error(
                compiler,
                "A class can't inherit from itself."
            );
        }

        jml_scope_begin(compiler);
        jml_local_add(compiler, jml_token_emit_synthetic(compiler->parser, "super"));
        jml_variable_definition(compiler, 0);

        jml_variable_named(compiler, class_name, false);
        jml_bytecode_emit_byte(compiler, OP_INHERIT);
        class_compiler.w_superclass = true;
    }

    jml_variable_named(compiler, class_name, false);
    jml_parser_consume(compiler, TOKEN_LBRACE, "Expect '{' before class body.");
    jml_parser_match_line(compiler);

    while (!jml_parser_check(compiler->parser, TOKEN_RBRACE)
        && !jml_parser_check(compiler->parser, TOKEN_EOF)) {

        if (jml_parser_match(compiler, TOKEN_FN)) {
            jml_method(compiler);

        } else if (jml_parser_match(compiler, TOKEN_LET)) {
            /*TODO*/

        } else if (jml_parser_match(compiler, TOKEN_CLASS)) {
            /*TODO*/

        } else {
            jml_parser_error(
                compiler,
                "Expect method declaration."
            );
            break;
        }
    }

    jml_parser_consume(compiler, TOKEN_RBRACE, "Expect '}' after class body.");
    jml_parser_newline(compiler, "Expect newline after 'class' declaration.");
    jml_bytecode_emit_byte(compiler, OP_POP);

    if (class_compiler.w_superclass) {
        jml_scope_end(compiler);
    }

    compiler->klass = compiler->klass->enclosing;
}


static void
jml_function_declaration(jml_compiler_t *compiler)
{
    uint16_t global = jml_variable_parse(compiler, "Expect function name.");
    jml_local_mark(compiler);

    jml_function(compiler, FUNCTION_FN);
    jml_variable_definition(compiler, global);
}


static void
jml_let_declaration(jml_compiler_t *compiler)
{
    uint16_t global = jml_variable_parse(compiler, "Expect variable name.");

    if (jml_parser_match(compiler, TOKEN_EQUAL))
        jml_expression(compiler);
    else
        jml_bytecode_emit_byte(compiler, OP_NONE);

    jml_parser_newline(compiler, "Expect newline after 'let' declaration.");
    jml_variable_definition(compiler, global);
}


static void
jml_declaration(jml_compiler_t *compiler)
{
    if (jml_parser_match(compiler, TOKEN_CLASS))
        jml_class_declaration(compiler);

    else if (jml_parser_match(compiler, TOKEN_FN))
        jml_function_declaration(compiler);

    else if (jml_parser_match(compiler, TOKEN_LET))
        jml_let_declaration(compiler);

    else
        jml_statement(compiler);

    if (compiler->parser->panicked)
        jml_parser_synchronize(compiler);
}


static void
jml_expression_statement(jml_compiler_t *compiler)
{
    jml_expression(compiler);
    jml_parser_newline(compiler, "Expect newline.");
    jml_bytecode_emit_byte(compiler, OP_POP);
}


static void
jml_return_statement(jml_compiler_t *compiler)
{
    if (compiler->type == FUNCTION_MAIN) {
        jml_parser_error(
            compiler,
            "Can't return from top-level code."
        );
    }

    if (jml_parser_match_line(compiler))
        jml_bytecode_emit_return(compiler);

    else {
        if (compiler->type == FUNCTION_INIT) {
            jml_parser_error(
                compiler,
                "Can't return a value from an initializer."
            );
        }

        jml_expression(compiler);
        jml_parser_match_line(compiler);
        jml_bytecode_emit_byte(compiler, OP_RETURN);
    }
}


static void
jml_import_statement(jml_compiler_t *compiler)
{
    bool    wildcard    = false;
    size_t  size        = JML_PATH_MAX;

    char   *fullname    = jml_realloc(NULL, size);
    size_t  length      = 0;

    char    name[JML_PATH_MAX];
    size_t  name_length = 0;

    jml_parser_consume(compiler, TOKEN_NAME, "Expect identifier after 'import'.");

    if (compiler->parser->previous.length >= JML_PATH_MAX) {
        jml_parser_error(compiler, "Module name too long.");
        jml_free(fullname);
        return;
    }

    memcpy(fullname, compiler->parser->previous.start, compiler->parser->previous.length);
    length              = compiler->parser->previous.length;

    memcpy(name, compiler->parser->previous.start, compiler->parser->previous.length);
    name_length         = compiler->parser->previous.length;

    if (jml_parser_match(compiler, TOKEN_DOT)) {
        do {
            if (jml_parser_match(compiler, TOKEN_STAR)) {
                wildcard = true;
                break;
            }

            jml_parser_consume(compiler, TOKEN_NAME, "Expect identifier after '.'.");

            if (compiler->parser->previous.length >= JML_PATH_MAX) {
                jml_parser_error(compiler, "Module name too long.");
                jml_free(fullname);
                return;
            }

            REALLOC(char, fullname, size, length + compiler->parser->previous.length + 1);
            fullname[length++] = '.';

            memcpy(fullname + length, compiler->parser->previous.start, compiler->parser->previous.length);
            length      += compiler->parser->previous.length;

            memcpy(name, compiler->parser->previous.start, compiler->parser->previous.length);
            name_length = compiler->parser->previous.length;
        } while (jml_parser_match(compiler, TOKEN_DOT));
    }

    fullname[length]  = '\0';
    name[name_length] = '\0';

    uint16_t full_arg = jml_bytecode_make_const(
        compiler,
        OBJ_VAL(jml_obj_string_take(fullname, length))
    );

    uint16_t name_arg = jml_bytecode_make_const(
        compiler,
        OBJ_VAL(jml_obj_string_copy(name, name_length))
    );

    if (wildcard) {
        if (compiler->type == FUNCTION_MAIN) {
            EMIT_EXTENDED_OP2(
                compiler, OP_IMPORT_WILDCARD, EXTENDED_OP(OP_IMPORT_WILDCARD), full_arg, name_arg
            );

        } else {
            jml_parser_error(
                compiler,
                "Can use wildcard import only in top-level code."
            );
        }

    } else if (compiler->type == FUNCTION_MAIN) {
        EMIT_EXTENDED_OP2(
            compiler, OP_IMPORT_GLOBAL, EXTENDED_OP(OP_IMPORT_GLOBAL), full_arg, name_arg
        );

        if (jml_parser_match(compiler, TOKEN_ARROW)) {
            jml_parser_consume(compiler, TOKEN_NAME, "Expect identifier after '->'.");
            uint16_t new_name = jml_identifier_const(compiler, &compiler->parser->previous);

            EMIT_EXTENDED_OP2(
                compiler, OP_SWAP_GLOBAL, EXTENDED_OP(OP_SWAP_GLOBAL), name_arg, new_name
            );
        }

    } else {
        jml_token_t token_name = jml_token_emit_synthetic(compiler->parser, name);
        int local = jml_local_resolve(compiler, &token_name);

        if (local == -1)
            local = jml_local_add_synthetic(compiler, &token_name);

        EMIT_EXTENDED_OP3(
            compiler, OP_IMPORT_LOCAL, EXTENDED_OP(OP_IMPORT_LOCAL), full_arg, name_arg, local
        );

        if (jml_parser_match(compiler, TOKEN_ARROW)) {
            jml_parser_consume(compiler, TOKEN_NAME, "Expect identifier after '->'.");

            memcpy(&compiler->locals[local].name,
                &compiler->parser->previous, sizeof(jml_token_t));
        }
    }

    jml_parser_newline(compiler, "Expect newline after 'import' statement.");
}


static void
jml_while_statement(jml_compiler_t *compiler)
{
    int start = jml_bytecode_current(compiler)->count;
    jml_parser_match_line(compiler);

    jml_expression(compiler);
    jml_parser_match_line(compiler);

    int exit = jml_bytecode_emit_jump(compiler, OP_JUMP_IF_FALSE);

    jml_loop_t loop;
    jml_loop_begin(
        compiler, &loop, start,
        jml_bytecode_current(compiler)->count, exit
    );

    jml_bytecode_emit_byte(compiler, OP_POP);

    jml_parser_consume(compiler, TOKEN_LBRACE, "Expect '{' after 'while'.");
    jml_block(compiler);

    jml_parser_newline(compiler, "Expect newline after 'while' block.");
    jml_bytecode_emit_loop(compiler, start);

    jml_bytecode_patch_jump(compiler, exit);
    jml_bytecode_emit_byte(compiler, OP_POP);

    jml_loop_end(compiler);
}


static void
jml_break_statement(jml_compiler_t *compiler)
{
    if (compiler->loop == NULL) {
        jml_parser_error(
            compiler,
            "Can't use 'break' outside of a loop."
        );
        return;
    }

    /*placeholder*/
    jml_bytecode_emit_jump(compiler, UINT8_MAX);
}


static void
jml_skip_statement(jml_compiler_t *compiler)
{
    if (compiler->loop == NULL) {
        jml_parser_error(
            compiler,
            "Can't use 'skip' outside of a loop."
        );
        return;
    }

    jml_bytecode_emit_loop(compiler, compiler->loop->start);
}


static void
jml_if_statement(jml_compiler_t *compiler)
{
    jml_expression(compiler);
    jml_parser_match_line(compiler);

    int then_jump = jml_bytecode_emit_jump(compiler, OP_JUMP_IF_FALSE);
    jml_bytecode_emit_byte(compiler, OP_POP);

    jml_parser_consume(compiler, TOKEN_LBRACE, "Expect '{' after 'if'.");
    jml_block(compiler);

    int else_jump = jml_bytecode_emit_jump(compiler, OP_JUMP);
    jml_bytecode_patch_jump(compiler, then_jump);
    jml_bytecode_emit_byte(compiler, OP_POP);

    if (jml_parser_match(compiler, TOKEN_ELSE)) {
        jml_parser_match_line(compiler);

        if (jml_parser_match(compiler, TOKEN_IF))
            jml_if_statement(compiler);

        else {
            jml_parser_consume(compiler, TOKEN_LBRACE, "Expect '{' after 'else'.");
            jml_block(compiler);
            jml_parser_newline(compiler, "Expect newline after 'else' block.");
        }

    } else
        jml_parser_newline(compiler, "Expect newline after 'if' block.");

    jml_bytecode_patch_jump(compiler, else_jump);
}


static void
jml_statement(jml_compiler_t *compiler)
{
    jml_parser_match_line(compiler);

    if (jml_parser_match(compiler, TOKEN_RETURN))
        jml_return_statement(compiler);

    else if (jml_parser_match(compiler, TOKEN_IMPORT))
        jml_import_statement(compiler);

    else if (jml_parser_match(compiler, TOKEN_WHILE))
        jml_while_statement(compiler);

    else if (jml_parser_match(compiler, TOKEN_BREAK))
        jml_break_statement(compiler);

    else if (jml_parser_match(compiler, TOKEN_SKIP))
        jml_skip_statement(compiler);

    else if (jml_parser_match(compiler, TOKEN_IF))
        jml_if_statement(compiler);

    else if (jml_parser_match(compiler, TOKEN_LBRACE)) {
        jml_scope_begin(compiler);
        jml_block(compiler);
        jml_scope_end(compiler);
        jml_parser_newline(compiler, "Expect newline after block.");

    } else
        jml_expression_statement(compiler);

    jml_parser_match_line(compiler);
}


jml_obj_function_t *
jml_compiler_compile(const char *source,
    jml_obj_module_t *module, bool output)
{
    jml_parser_t    parser;
    jml_compiler_t  compiler;

    jml_lexer_init(&parser.lexer, source);
    jml_compiler_init(&compiler, NULL, &parser,
        FUNCTION_MAIN, module, output);

    parser.w_error  = false;
    parser.panicked = false;

    jml_parser_advance(&compiler);
    jml_parser_match_line(&compiler);

    while (!jml_parser_match(&compiler, TOKEN_EOF))
        jml_declaration(&compiler);

    jml_obj_function_t *function = jml_compiler_end(&compiler);
    return parser.w_error ? NULL : function;
}


void
jml_compiler_mark(jml_compiler_t *compiler)
{
    jml_compiler_t *current = compiler;

    while (current != NULL) {
        jml_gc_mark_obj((jml_obj_t*)current->function);
        current = current->enclosing;
    }
}
