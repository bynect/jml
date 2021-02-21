#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jml_compiler.h>
#include <jml_gc.h>
#include <jml_util.h>
#include <jml_string.h>


static jml_parser_t                 parser;

static jml_compiler_t              *current         = NULL;

static jml_class_compiler_t        *class_current   = NULL;


static inline jml_bytecode_t *
jml_bytecode_current(void)
{
    return &current->function->bytecode;
}


static jml_token_t
jml_token_emit_synthetic(const char *text)
{
    jml_token_t token;
    token.start     = text;
    token.length    = strlen(text);
    token.line      = parser.current.line;
    token.offset    = 0;

    return token;
}


static void
jml_parser_error_at(jml_token_t *token,
    const char *message)
{
    if (parser.panicked)
        return;

    parser.panicked = true;

    if (current->output) {
        fprintf(stderr, "[line %d", token->line);

        if (current->module != NULL) {
            fprintf(stderr, " in module %s",
                current->module->name->chars);
        }

        fprintf(stderr, "] Error");

        if (token->type == TOKEN_EOF) {
            fprintf(stderr, " at eof");

        } else if (token->type == TOKEN_ERROR) {
            fprintf(stderr, " at '%c'",
                parser.lexer.source[token->offset]);

        } else if (strncmp(token->start, "\n", token->length) == 0) {
            fprintf(stderr, " at newline");

        } else {
            fprintf(stderr, " at '%.*s'",
                token->length, token->start);
        }

        fprintf(stderr, ": %s\n", message);
    }

    parser.w_error = true;
}


#define jml_parser_error(message)                       \
    jml_parser_error_at(&parser.previous, message)


#define jml_parser_error_current(message)               \
    jml_parser_error_at(&parser.current, message)


static void
jml_parser_advance(void)
{
    parser.previous = parser.current;

    while (true) {
        parser.current = jml_lexer_tokenize(&parser.lexer);

#ifdef JML_PRINT_TOKEN
        jml_token_type_print(parser.current.type);
        printf("    %.*s\n", parser.current.length, parser.current.start);
#endif
        if (parser.current.type != TOKEN_ERROR)
            break;

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


static inline bool
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


static bool
jml_parser_match_line(void)
{
    if (!jml_parser_match(TOKEN_LINE))
        return false;

    while (jml_parser_match(TOKEN_LINE));
    return true;
}


static void
jml_parser_newline(const char *message)
{
    if (!jml_parser_check(TOKEN_RBRACE))
        jml_parser_consume(TOKEN_LINE, message);

    jml_parser_match_line();
}


static uint8_t
jml_bytecode_make_const(jml_value_t value)
{
    int constant = jml_bytecode_add_const(
        jml_bytecode_current(), value
    );

    if (constant > UINT8_MAX) {
        jml_parser_error("Too many constants in one chunk.");
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
jml_bytecode_emit_bytes(uint8_t byte1, uint8_t byte2)
{
    jml_bytecode_emit_byte(byte1);
    jml_bytecode_emit_byte(byte2);
}


static void
jml_bytecode_emit_loop(int start)
{
    jml_bytecode_emit_byte(OP_LOOP);

    int offset = jml_bytecode_current()->count - start + 2;
    if (offset > UINT16_MAX) jml_parser_error("Loop body too large.");

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
        jml_parser_error("Too much code to jump over.");
    }

    jml_bytecode_current()->code[offset] = (jump >> 8) & 0xff;
    jml_bytecode_current()->code[offset + 1] = jump & 0xff;
}


static void
jml_bytecode_emit_return(void)
{
    if (current->type == FUNCTION_INIT)
        jml_bytecode_emit_bytes(OP_GET_LOCAL, 0);
    else
        jml_bytecode_emit_byte(OP_NONE);

    jml_bytecode_emit_byte(OP_RETURN);
}


static void
jml_bytecode_emit_const(jml_value_t value)
{
    jml_bytecode_emit_bytes(
        OP_CONST, jml_bytecode_make_const(value)
    );
}


static void
jml_compiler_init(jml_compiler_t *compiler, jml_function_type type,
    jml_obj_module_t *module, bool output)
{
    compiler->enclosing     = current;
    compiler->function      = NULL;
    compiler->type          = type;

    compiler->module        = module;

    compiler->local_count   = 0;
    compiler->scope_depth   = 0;
    compiler->function      = jml_obj_function_new();
    current                 = compiler;

    if (type != FUNCTION_MAIN) {
        current->function->name = jml_obj_string_copy(
            parser.previous.start, parser.previous.length
        );
    }

    if (type == FUNCTION_METHOD || type == FUNCTION_INIT) {
        compiler->function->klass_name = jml_obj_string_copy(
            class_current->name.start, class_current->name.length
        );
    }

    jml_local_t *local      = &current->locals[current->local_count++];
    local->depth            = 0;

    if (type == FUNCTION_METHOD || type == FUNCTION_INIT) {
        local->name.start   = "self";
        local->name.length  = 4;
    } else {
        local->name.start   = "";
        local->name.length  = 0;
    }

    local->captured         = false;

    compiler->loop          = NULL;

    compiler->output        = output;
}


static jml_obj_function_t *
jml_compiler_end(void)
{
    jml_bytecode_emit_return();
    jml_obj_function_t *function = current->function;

#ifdef JML_DISASSEMBLE
    if (!parser.w_error) {
        jml_bytecode_disassemble(jml_bytecode_current(),
            function->name != NULL ? function->name->chars : "__main");
    }
#endif

    current = current->enclosing;
    return function;
}


static void
jml_scope_begin(void)
{
    current->scope_depth++;
}


static void
jml_scope_end(void)
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


static inline uint8_t
jml_identifier_const(jml_token_t *name) {
    return jml_bytecode_make_const(OBJ_VAL(
        jml_obj_string_copy(name->start, name->length)
    ));
}


static bool
jml_identifier_equal(jml_token_t *a, jml_token_t *b)
{
    if (a->length != b->length)
        return false;

    return memcmp(a->start, b->start, a->length) == 0;
}


static inline void
jml_local_mark(void)
{
    current->locals[current->local_count - 1].depth = current->scope_depth;
}


static inline void
jml_local_unmark(void)
{
    current->locals[current->local_count - 1].depth = -1;
}


static void
jml_local_add(jml_token_t name)
{
    if (current->local_count == LOCAL_MAX) {
        jml_parser_error("Too many local variables in function.");
        return;
    }

    jml_local_t *local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
    local->captured = false;
}


static int
jml_local_resolve(jml_compiler_t *compiler,
    jml_token_t *name)
{
    for (int i = compiler->local_count - 1; i >= 0; --i) {
        jml_local_t *local = &compiler->locals[i];
        if (jml_identifier_equal(name, &local->name)) {
            if (local->depth == -1) {
                jml_parser_error(
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
    jml_local_add(*name);
    jml_local_mark();

    return jml_local_resolve(compiler, name);
}

static int
jml_upvalue_add(jml_compiler_t *compiler,
    uint8_t index, bool local)
{
    int upvalue_count = compiler->function->upvalue_count;
    for (int i = 0; i < upvalue_count; i++) {
        jml_upvalue_t *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->local == local) {
            return i;
        }
    }

    if (upvalue_count == LOCAL_MAX) {
        jml_parser_error("Too many upvalues in function.");
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
jml_loop_begin(int start, int body, int exit)
{
    jml_loop_t  *loop   = jml_alloc(sizeof(jml_loop_t));

    loop->enclosing     = current->loop;
    loop->start         = start;
    loop->body          = body;
    loop->exit          = exit;

    current->loop = loop;
}


static void
jml_loop_end(void)
{
    jml_loop_t  *loop   = current->loop;

    if (loop != NULL) {
        int count       = jml_bytecode_current()->count;
        for (int i = loop->body; i < count; ) {
            if (jml_bytecode_current()->code[i] == UINT8_MAX) {
                jml_bytecode_current()->code[i] = OP_JUMP;
                jml_bytecode_patch_jump(i + 1);
                i += 3;
            } else
                ++i;
        }

        current->loop   = loop->enclosing;
        jml_free(loop);
    }
}


static void
jml_variable_declaration(void)
{
    if (current->scope_depth == 0)
        return;

    jml_token_t *name = &parser.previous;

    for (int i = current->local_count - 1; i >= 0; --i) {
        jml_local_t *local = &current->locals[i];
        if (local->depth != -1
            && local->depth < current->scope_depth) {
            break;
        }

        if (jml_identifier_equal(name, &local->name)) {
            jml_parser_error(
                "Variable alredy declared in this scope."
            );
        }
    }

    jml_local_add(*name);
}


static uint8_t
jml_variable_parse(const char *message)
{
    jml_parser_consume(TOKEN_NAME, message);

    jml_variable_declaration();
    if (current->scope_depth > 0)
        return 0;

    return jml_identifier_const(&parser.previous);
}


static void
jml_variable_definition(uint8_t global)
{
    if (current->scope_depth > 0) {
        jml_local_mark();
        return;
    }

    jml_bytecode_emit_bytes(OP_DEF_GLOBAL, global);
}


/*forwarded declaration*/
static void jml_expression(void);

static void jml_statement(void);

static void jml_declaration(void);

static jml_parser_rule *jml_parser_rule_get(jml_token_type type);

static void jml_parser_precedence_parse(jml_parser_precedence precedence);


static void
jml_parser_synchronize(void)
{
    parser.panicked = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_LINE) {
            jml_parser_match_line();
            return;
        }

        switch (parser.current.type) {
            case TOKEN_FOR:
            case TOKEN_WHILE:
            case TOKEN_BREAK:
            case TOKEN_SKIP:
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

        jml_parser_advance();
    }
}


static uint8_t
jml_arguments_list(void)
{
    jml_parser_match_line();

    uint8_t arg_count = 0;
    if (!jml_parser_check(TOKEN_RPAREN)) {
        do {
            jml_parser_match_line();

            jml_expression();
            if (arg_count == 255)
                jml_parser_error("Can't have more than 255 arguments.");

            arg_count++;
        } while (jml_parser_match(TOKEN_COMMA));
    }

    jml_parser_match_line();
    jml_parser_consume(TOKEN_RPAREN, "Expect ')' after arguments.");
    return arg_count;
}


static void
jml_and(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    int jump_end = jml_bytecode_emit_jump(OP_JUMP_IF_FALSE);

    jml_bytecode_emit_byte(OP_POP);
    jml_parser_precedence_parse(PREC_AND);

    jml_bytecode_patch_jump(jump_end);
}


static void
jml_or(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    int jump_else = jml_bytecode_emit_jump(OP_JUMP_IF_FALSE);
    int jump_end = jml_bytecode_emit_jump(OP_JUMP);

    jml_bytecode_patch_jump(jump_else);
    jml_bytecode_emit_byte(OP_POP);

    jml_parser_precedence_parse(PREC_OR);
    jml_bytecode_patch_jump(jump_end);
}


static void
jml_binary(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    jml_token_type operator_token = parser.previous.type;

    jml_parser_rule *rule = jml_parser_rule_get(operator_token);
    jml_parser_precedence_parse(
        (jml_parser_precedence)(rule->precedence + 1)
    );

    switch (operator_token) {
        case TOKEN_PLUS:        jml_bytecode_emit_byte(OP_ADD);         break;
        case TOKEN_MINUS:       jml_bytecode_emit_byte(OP_SUB);         break;
        case TOKEN_STAR:        jml_bytecode_emit_byte(OP_MUL);         break;
        case TOKEN_SLASH:       jml_bytecode_emit_byte(OP_DIV);         break;
        case TOKEN_STARSTAR:    jml_bytecode_emit_byte(OP_POW);         break;
        case TOKEN_PERCENT:     jml_bytecode_emit_byte(OP_MOD);         break;

        case TOKEN_EQEQUAL:     jml_bytecode_emit_byte(OP_EQUAL);       break;
        case TOKEN_GREATER:     jml_bytecode_emit_byte(OP_GREATER);     break;
        case TOKEN_GREATEREQ:   jml_bytecode_emit_byte(OP_GREATEREQ);   break;
        case TOKEN_LESS:        jml_bytecode_emit_byte(OP_LESS);        break;
        case TOKEN_LESSEQ:      jml_bytecode_emit_byte(OP_LESSEQ);      break;
        case TOKEN_NOTEQ:       jml_bytecode_emit_byte(OP_NOTEQ);       break;

        case TOKEN_IN:          jml_bytecode_emit_byte(OP_CONTAIN);     break;

        default:                JML_UNREACHABLE();
    }
}


static void
jml_call(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    uint8_t arg_count = jml_arguments_list();
    jml_bytecode_emit_bytes(OP_CALL, arg_count);
}


static void
jml_dot(bool assignable)
{
    jml_parser_match_line();

    jml_parser_consume(TOKEN_NAME, "Expect identifier after '.'.");
    uint8_t name = jml_identifier_const(&parser.previous);

    if (assignable && jml_parser_match(TOKEN_EQUAL)) {
        jml_parser_match_line();
        jml_expression();
        jml_bytecode_emit_bytes(OP_SET_MEMBER, name);

    } else if (jml_parser_match(TOKEN_LPAREN)) {
        uint8_t arg_count = jml_arguments_list();
        jml_bytecode_emit_bytes(OP_INVOKE, name);
        jml_bytecode_emit_byte(arg_count);

    } else
        jml_bytecode_emit_bytes(OP_GET_MEMBER, name);
}


static void
jml_grouping(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();
    jml_expression();
    jml_parser_match_line();

    jml_parser_consume(TOKEN_RPAREN, "Expect ')' after expression.");
}


static void
jml_indexing(bool assignable)
{
    jml_parser_match_line();
    jml_expression();
    jml_parser_match_line();

    jml_parser_consume(TOKEN_RSQARE, "Expect ']' after indexing.");

    if (assignable && jml_parser_match(TOKEN_EQUAL)) {
        jml_parser_match_line();
        jml_expression();
        jml_bytecode_emit_byte(OP_SET_INDEX);

    } else
        jml_bytecode_emit_byte(OP_GET_INDEX);
}


static void
jml_literal(JML_UNUSED(bool assignable))
{
    switch (parser.previous.type) {
        case TOKEN_FALSE:       jml_bytecode_emit_byte(OP_FALSE);       break;
        case TOKEN_NONE:        jml_bytecode_emit_byte(OP_NONE);        break;
        case TOKEN_TRUE:        jml_bytecode_emit_byte(OP_TRUE);        break;

        default:                JML_UNREACHABLE();
    }
}


static void
jml_number(JML_UNUSED(bool assignable))
{
    double value = strtod(parser.previous.start, NULL);
    jml_bytecode_emit_const(NUM_VAL(value));
}


static void
jml_string(JML_UNUSED(bool assignable))
{
    const char *raw             = parser.previous.start + 1;
    size_t length               = parser.previous.length - 2;

    char *buffer                = ALLOCATE(char, length + 1);
    size_t size                 = 0;

    for (size_t i = 0; i < length; ) {
        uint32_t c = raw[i];

        if (c == '\\') {
            if (i + 1 >= length) {
                jml_parser_error_current("Invalid string escape sequence.");
                break;
            }

            switch (raw[i + 1]) {
                case '\'':              c = '\''; ++i;                          break;
                case  '"':              c = '"';  ++i;                          break;
                case '\\':              c = '\\'; ++i;                          break;
                case  'n':              c = '\n'; ++i;                          break;
                case  'r':              c = '\r'; ++i;                          break;
                case  't':              c = '\t'; ++i;                          break;

                case 'x': {
                    if (i + 3 >= length) {
                        jml_parser_error_current("Invalid string escape sequence.");
                        break;
                    }

                    if (!jml_is_hex(raw[i + 2]) || !jml_is_hex(raw[i + 3])) {
                        jml_parser_error_current("Invalid string escape sequence.");
                        break;
                    }

                    char hex[3] = {
                        raw[i + 2],
                        raw[i + 3],
                        0
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

                case 'u':  {
                    if (i + 5 >= length) {
                        jml_parser_error_current("Invalid string escape sequence.");
                        break;
                    }

                    if (!jml_is_hex(raw[i + 2]) || !jml_is_hex(raw[i + 3])
                        || !jml_is_hex(raw[i + 4]) || !jml_is_hex(raw[i + 5])) {
                        jml_parser_error_current("Invalid string escape sequence.");
                        break;
                    }

                    char hex[5] = {
                        raw[i + 2],
                        raw[i + 3],
                        raw[i + 4],
                        raw[i + 5],
                        0
                    };

                    uint32_t value = strtoul(hex, NULL, 16);
                    size += jml_string_encode(&buffer[size], value);
                    i += 6;
                    continue;
                }

                case 'U':  {
                    if (i + 9 >= length) {
                        jml_parser_error_current("Invalid string escape sequence.");
                        break;
                    }

                    if (!jml_is_hex(raw[i + 2]) || !jml_is_hex(raw[i + 3])
                        || !jml_is_hex(raw[i + 4]) || !jml_is_hex(raw[i + 5])
                        || !jml_is_hex(raw[i + 6]) || !jml_is_hex(raw[i + 7])
                        || !jml_is_hex(raw[i + 8]) || !jml_is_hex(raw[i + 9])) {
                        jml_parser_error_current("Invalid string escape sequence.");
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
                        0
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
        OBJ_VAL(jml_obj_string_take(buffer, size))
    );
}


static void
jml_array(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    uint8_t item_count = 0;
    if (!jml_parser_check(TOKEN_RSQARE)) {
        do {
            jml_parser_match_line();
            jml_expression();

            if (item_count == 255) {
                jml_parser_error("Can't have more than 255 items in an array.");
            }

            jml_parser_match_line();
            ++item_count;
        } while (jml_parser_match(TOKEN_COMMA));
    }

    jml_parser_match_line();
    jml_parser_consume(TOKEN_RSQARE, "Expect ']' after array.");
    jml_bytecode_emit_bytes(OP_ARRAY, item_count);
}


static void
jml_map(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    uint8_t item_count = 0;
    if (!jml_parser_check(TOKEN_RBRACE)) {
        do {
            jml_parser_match_line();
            jml_parser_advance();
            jml_string(true);

            jml_parser_match_line();
            jml_parser_consume(TOKEN_COLON, "Expect colon in map.");

            jml_parser_match_line();
            jml_expression();

            if (item_count == 254) {
                jml_parser_error("Can't have more than 254 items in map.");
            }

            jml_parser_match_line();
            item_count += 2;
        } while (jml_parser_match(TOKEN_COMMA));
    }

    jml_parser_match_line();
    jml_parser_consume(TOKEN_RBRACE, "Expect '}' after map.");
    jml_bytecode_emit_bytes(OP_MAP, item_count);
}


static void
jml_variable_assignment(bool index, uint8_t get_op,
    uint8_t set_op, uint8_t arg, uint8_t op)
{
    if (index) {
        jml_bytecode_emit_byte(OP_SAVE);
        jml_bytecode_emit_bytes(get_op, arg);
        jml_bytecode_emit_bytes(OP_ROT, OP_GET_INDEX);

        jml_parser_match_line();
        jml_expression();

        jml_bytecode_emit_bytes(op, OP_SET_INDEX);
    } else {
        jml_bytecode_emit_bytes(get_op, arg);
        jml_parser_match_line();
        jml_expression();

        jml_bytecode_emit_bytes(op, set_op);
        jml_bytecode_emit_byte(arg);
    }
}


static void
jml_variable_named(jml_token_t name, bool assignable)
{
    uint8_t get_op, set_op;
    bool index  = false;
    int  arg    = jml_local_resolve(current, &name);

    if (arg != -1) {
        get_op  = OP_GET_LOCAL;
        set_op  = OP_SET_LOCAL;

    } else if ((arg = jml_upvalue_resolve(current, &name)) != -1) {
        get_op  = OP_GET_UPVALUE;
        set_op  = OP_SET_UPVALUE;

    } else {
        arg     = jml_identifier_const(&name);
        get_op  = OP_GET_GLOBAL;
        set_op  = OP_SET_GLOBAL;
    }

    if (jml_parser_match(TOKEN_LSQARE)) {
        jml_bytecode_emit_bytes(get_op, (uint8_t)arg);

        jml_parser_match_line();
        jml_expression();

        jml_parser_match_line();
        jml_parser_consume(TOKEN_RSQARE, "Expect ']' after indexing.");

        index = true;
    }

    if (assignable && jml_parser_match(TOKEN_EQUAL)) {
        jml_parser_match_line();
        jml_expression();

        if (index)
            jml_bytecode_emit_byte(OP_SET_INDEX);
        else
            jml_bytecode_emit_bytes(set_op, (uint8_t)arg);

    } else if (assignable && jml_parser_match(TOKEN_PLUSEQ)) {
        jml_variable_assignment(
            index, get_op, set_op, (uint8_t)arg, OP_ADD
        );

    } else if (assignable && jml_parser_match(TOKEN_MINUSEQ)) {
        jml_variable_assignment(
            index, get_op, set_op, (uint8_t)arg, OP_SUB
        );

    } else if (assignable && jml_parser_match(TOKEN_STAREQ)) {
        jml_variable_assignment(
            index, get_op, set_op, (uint8_t)arg, OP_MUL
        );

    } else if (assignable && jml_parser_match(TOKEN_STARSTAREQ)) {
        jml_variable_assignment(
            index, get_op, set_op, (uint8_t)arg, OP_POW
        );

    } else if (assignable && jml_parser_match(TOKEN_SLASHEQ)) {
        jml_variable_assignment(
            index, get_op, set_op, (uint8_t)arg, OP_DIV
        );

    } else if (assignable && jml_parser_match(TOKEN_PERCENTEQ)) {
        jml_variable_assignment(
            index, get_op, set_op, (uint8_t)arg, OP_MOD
        );

    } else if (jml_parser_match(TOKEN_ARROW)) {
        if (!index) {
            jml_parser_match_line();
            jml_parser_consume(TOKEN_NAME, "Expect identifier after '->'.");

            jml_bytecode_emit_byte(OP_NONE);

            if (get_op == OP_GET_GLOBAL) {
                jml_bytecode_emit_bytes(OP_SWAP_GLOBAL, (uint8_t)arg);
                jml_bytecode_emit_byte(jml_identifier_const(&parser.previous));
            } else
                memcpy(&current->locals[arg].name, &parser.previous, sizeof(jml_token_t));
        } else
            jml_parser_error("Can't swap indexed value.");

    } else if (index)
        jml_bytecode_emit_byte(OP_GET_INDEX);
    else
        jml_bytecode_emit_bytes(get_op, (uint8_t)arg);
}


static void
jml_variable(bool assignable)
{
    jml_variable_named(parser.previous, assignable);
}


static void
jml_super(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    if (class_current == NULL) {
        jml_parser_error("Can't use 'super' outside of a class.");
    } else if (!class_current->w_superclass) {
        jml_parser_error("Can't use 'super' in a class with no superclass.");
    }

    jml_parser_consume(TOKEN_DOT, "Expect '.' after 'super'.");
    jml_parser_consume(TOKEN_NAME, "Expect superclass method name.");

    uint8_t name = jml_identifier_const(&parser.previous);
    jml_variable_named(jml_token_emit_synthetic("self"), false);

    if (jml_parser_match(TOKEN_LPAREN)) {
        uint8_t arg_count = jml_arguments_list();

        jml_variable_named(jml_token_emit_synthetic("super"), false);
        jml_bytecode_emit_bytes(OP_SUPER_INVOKE, name);
        jml_bytecode_emit_byte(arg_count);
    } else {
        jml_variable_named(jml_token_emit_synthetic("super"), false);
        jml_bytecode_emit_bytes(OP_SUPER, name);
    }
}


static void
jml_self(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    if (class_current == NULL) {
        jml_parser_error("Can't use 'self' outside of a class.");
        return;
    }
    jml_variable(false);
}


static void
jml_unary(JML_UNUSED(bool assignable))
{
    jml_parser_match_line();

    jml_token_type type = parser.previous.type;
    jml_parser_precedence_parse(PREC_UNARY);

    switch (type) {
        case TOKEN_NOT:         jml_bytecode_emit_byte(OP_NOT);       break;
        case TOKEN_MINUS:       jml_bytecode_emit_byte(OP_NEGATE);    break;

        default:                JML_UNREACHABLE();
    }
}


static jml_parser_rule rules[] = {
    /*TOKEN_RPAREN*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_LPAREN*/    {&jml_grouping, &jml_call,      PREC_CALL},
    /*TOKEN_RSQARE*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_LSQARE*/    {&jml_array,    &jml_indexing,  PREC_CALL},
    /*TOKEN_RBRACE*/    {NULL,          NULL,           PREC_NONE},
    /*TOKEN_LBRACE*/    {&jml_map,      NULL,           PREC_NONE},

    /*TOKEN_COLON*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_SEMI*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_COMMA*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_DOT*/       {NULL,          &jml_dot,       PREC_CALL},

    /*TOKEN_PIPE*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_CARET*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_AMP*/       {NULL,          NULL,           PREC_NONE},
    /*TOKEN_TILDE*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_QUEST*/     {NULL,          NULL,           PREC_NONE},
    /*TOKEN_BANG*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_HASH*/      {NULL,          NULL,           PREC_NONE},
    /*TOKEN_AT*/        {NULL,          NULL,           PREC_NONE},
    /*TOKEN_ARROW*/     {NULL,          NULL,           PREC_NONE},

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
    return &rules[type];
}


static void
jml_parser_precedence_parse(
    jml_parser_precedence precedence)
{
    jml_parser_advance();

    jml_parser_fn prefix_rule = jml_parser_rule_get(
        parser.previous.type)->prefix;

    if (prefix_rule == NULL) {
        jml_parser_error("Expect expression.");
        return;
    }

    bool assignable = precedence <= PREC_ASSIGNMENT;
    prefix_rule(assignable);

    while (precedence <= jml_parser_rule_get(
        parser.current.type)->precedence) {

        jml_parser_advance();
        jml_parser_fn infix_rule = jml_parser_rule_get(
            parser.previous.type)->infix;

        infix_rule(assignable);
    }

    if (assignable
        && (jml_parser_match(TOKEN_EQUAL)
        || jml_parser_match(TOKEN_PLUSEQ)
        || jml_parser_match(TOKEN_MINUSEQ)
        || jml_parser_match(TOKEN_STAREQ)
        || jml_parser_match(TOKEN_STARSTAREQ)
        || jml_parser_match(TOKEN_SLASHEQ)
        || jml_parser_match(TOKEN_PERCENTEQ))) {

        jml_parser_error("Invalid assignment target.");
    }
}


static void
jml_expression(void)
{
    jml_parser_precedence_parse(PREC_ASSIGNMENT);
}


static void
jml_block(void)
{
    jml_parser_match_line();

    while (!jml_parser_check(TOKEN_RBRACE)
        && !jml_parser_check(TOKEN_EOF))
        jml_declaration();

    jml_parser_consume(TOKEN_RBRACE, "Expect '}' after block.");
}


static void
jml_function(jml_function_type type)
{
    jml_compiler_t compiler;
    jml_compiler_init(&compiler, type, current->module, current->output);
    jml_scope_begin();

    jml_parser_consume(TOKEN_LPAREN, "Expect '(' after function name.");
    if (!jml_parser_check(TOKEN_RPAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                jml_parser_error_current(
                    "Can't have more than 255 parameters."
                );
            }

            uint8_t param_const = jml_variable_parse("Expect parameter name.");
            jml_variable_definition(param_const);
        } while (jml_parser_match(TOKEN_COMMA));
    }

    jml_parser_consume(TOKEN_RPAREN, "Expect ')' after parameters.");
    jml_parser_consume(TOKEN_LBRACE, "Expect '{' before function body.");
    jml_block();
    jml_parser_newline("Expect newline after 'fn' declaration.");

    jml_obj_function_t *function = jml_compiler_end();
    jml_bytecode_emit_bytes(
        OP_CLOSURE, jml_bytecode_make_const(OBJ_VAL(function))
    );

    for (int i = 0; i < function->upvalue_count; i++) {
        jml_bytecode_emit_byte(compiler.upvalues[i].local ? 1 : 0);
        jml_bytecode_emit_byte(compiler.upvalues[i].index);
    }
}


static void
jml_method(void)
{
    jml_parser_consume(TOKEN_FN, "Expect 'fn' in method declaration.");
    jml_parser_match_line();
    jml_parser_consume(TOKEN_NAME, "Expect method name.");
    uint8_t constant = jml_identifier_const(&parser.previous);

    jml_function_type type = FUNCTION_METHOD;

    if (parser.previous.length == 6 &&
        memcmp(parser.previous.start, "__init", 6) == 0)
        type = FUNCTION_INIT;

    jml_function(type);
    jml_bytecode_emit_bytes(OP_METHOD, constant);
}


static void
jml_class_declaration(void)
{
    jml_parser_match_line();

    jml_parser_consume(TOKEN_NAME, "Expect class name.");
    jml_token_t class_name = parser.previous;
    uint8_t name_const = jml_identifier_const(&parser.previous);
    jml_variable_declaration();

    jml_bytecode_emit_bytes(OP_CLASS, name_const);
    jml_variable_definition(name_const);

    jml_class_compiler_t class_compiler;
    class_compiler.name = parser.previous;
    class_compiler.enclosing = class_current;
    class_compiler.w_superclass = false;
    class_current = &class_compiler;

    if (jml_parser_match(TOKEN_LESS)) {
        jml_parser_match_line();

        jml_parser_consume(TOKEN_NAME, "Expect superclass name.");
        jml_variable(false);

        while (jml_parser_match(TOKEN_DOT)) {
            jml_parser_consume(TOKEN_NAME, "Expect identifier after '.'.");
            uint8_t name = jml_identifier_const(&parser.previous);
            jml_bytecode_emit_bytes(OP_GET_MEMBER, name);
        }

        if (jml_identifier_equal(&class_name, &parser.previous)) {
            jml_parser_error("A class can't inherit from itself.");
        }

        jml_scope_begin();
        jml_local_add(jml_token_emit_synthetic("super"));
        jml_variable_definition(0);

        jml_variable_named(class_name, false);
        jml_bytecode_emit_byte(OP_INHERIT);
        class_compiler.w_superclass = true;
    }

    jml_variable_named(class_name, false);
    jml_parser_consume(TOKEN_LBRACE, "Expect '{' before class body.");
    jml_parser_match_line();

    while (!jml_parser_check(TOKEN_RBRACE)
        && !jml_parser_check(TOKEN_EOF)) {
        jml_method();
    }

    jml_parser_consume(TOKEN_RBRACE, "Expect '}' after class body.");
    jml_parser_newline("Expect newline after 'class' declaration.");
    jml_bytecode_emit_byte(OP_POP);

    if (class_compiler.w_superclass) {
        jml_scope_end();
    }

    class_current = class_current->enclosing;
}


static void
jml_function_declaration(void)
{
    uint8_t global = jml_variable_parse("Expect function name.");
    jml_local_mark();
    jml_function(FUNCTION_FN);
    jml_variable_definition(global);
}


static void
jml_let_declaration(void)
{
    uint8_t global = jml_variable_parse("Expect variable name.");

    if (jml_parser_match(TOKEN_EQUAL))
        jml_expression();
    else
        jml_bytecode_emit_byte(OP_NONE);

    jml_parser_newline("Expect newline after 'let' declaration.");
    jml_variable_definition(global);
}


static void
jml_declaration(void)
{
    if (jml_parser_match(TOKEN_CLASS))
        jml_class_declaration();

    else if (jml_parser_match(TOKEN_FN))
        jml_function_declaration();

    else if (jml_parser_match(TOKEN_LET))
        jml_let_declaration();

    else
        jml_statement();

    if (parser.panicked)
        jml_parser_synchronize();
}


static void
jml_expression_statement(void)
{
    jml_expression();
    jml_parser_newline("Expect newline.");
    jml_bytecode_emit_byte(OP_POP);
}


static void
jml_return_statement(void)
{
    if (current->type == FUNCTION_MAIN) {
        jml_parser_error("Can't return from top-level code.");
    }

    if (jml_parser_match_line())
        jml_bytecode_emit_return();
    else {
        if (current->type == FUNCTION_INIT)
            jml_parser_error("Can't return a value from an initializer.");

        jml_expression();
        jml_parser_match_line();
        jml_bytecode_emit_byte(OP_RETURN);
    }
}


static void
jml_import_statement(void)
{
    bool    wildcard    = false;
    size_t  size        = JML_PATH_MAX;

    char   *fullname    = jml_realloc(NULL, size);
    size_t  length      = 0;

    char    name[JML_PATH_MAX];
    size_t  name_length = 0;

    jml_parser_consume(TOKEN_NAME, "Expect identifier after 'import'.");

    if (parser.previous.length >= JML_PATH_MAX) {
        jml_parser_error("Module name too long.");
        jml_free(fullname);
        return;
    }

    memcpy(fullname, parser.previous.start, parser.previous.length);
    length = parser.previous.length;

    memcpy(name, parser.previous.start, parser.previous.length);
    name_length = parser.previous.length;

    if (jml_parser_match(TOKEN_DOT)) {
        do {
            if (jml_parser_match(TOKEN_STAR)) {
                wildcard = true;
                break;
            }

            jml_parser_consume(TOKEN_NAME, "Expect identifier after '.'.");

            if (parser.previous.length >= JML_PATH_MAX) {
                jml_parser_error("Module name too long.");
                jml_free(fullname);
                return;
            }

            REALLOC(char, fullname, size, length + parser.previous.length + 1);
            fullname[length++] = '.';

            memcpy(fullname + length, parser.previous.start, parser.previous.length);
            length += parser.previous.length;

            memcpy(name, parser.previous.start, parser.previous.length);
            name_length = parser.previous.length;
        } while (jml_parser_match(TOKEN_DOT));
    }

    fullname[length]  = '\0';
    name[name_length] = '\0';

    uint8_t full_arg = jml_bytecode_make_const(OBJ_VAL(
        jml_obj_string_take(fullname, length)
    ));

    uint8_t name_arg = jml_bytecode_make_const(OBJ_VAL(
        jml_obj_string_copy(name, name_length)
    ));

    if (wildcard) {
        if (current->type == FUNCTION_MAIN) {
            jml_bytecode_emit_bytes(OP_IMPORT_WILDCARD, full_arg);
            jml_bytecode_emit_byte(name_arg);
        } else
            jml_parser_error("Can use wildcard import only in top-level code.");

    } else if (current->type == FUNCTION_MAIN) {
        jml_bytecode_emit_bytes(OP_IMPORT_GLOBAL, full_arg);
        jml_bytecode_emit_byte(name_arg);

        if (jml_parser_match(TOKEN_ARROW)) {
            jml_parser_match_line();
            jml_parser_consume(TOKEN_NAME, "Expect identifier after '->'.");
            uint8_t new_name = jml_identifier_const(&parser.previous);

            jml_bytecode_emit_bytes(OP_SWAP_GLOBAL, name_arg);
            jml_bytecode_emit_byte(new_name);
        }

    } else {
        jml_token_t token_name = jml_token_emit_synthetic(name);
        int local = jml_local_resolve(current, &token_name);

        if (local == -1)
            local = jml_local_add_synthetic(current, &token_name);

        jml_bytecode_emit_bytes(OP_IMPORT_LOCAL, full_arg);
        jml_bytecode_emit_bytes(name_arg, local);

        if (jml_parser_match(TOKEN_ARROW)) {
            jml_parser_match_line();
            jml_parser_consume(TOKEN_NAME, "Expect identifier after '->'.");

            memcpy(&current->locals[local].name,
                &parser.previous, sizeof(jml_token_t));
        }
    }
}


static void
jml_while_statement(void)
{
    int start = jml_bytecode_current()->count;

    jml_parser_match_line();
    jml_expression();
    jml_parser_match_line();

    int exit = jml_bytecode_emit_jump(OP_JUMP_IF_FALSE);
    jml_loop_begin(start, jml_bytecode_current()->count, exit);

    jml_bytecode_emit_byte(OP_POP);

    jml_parser_consume(TOKEN_LBRACE, "Expect '{' after 'while'.");
    jml_block();
    jml_parser_newline("Expect newline after 'while' block.");

    jml_bytecode_emit_loop(start);

    jml_bytecode_patch_jump(exit);
    jml_bytecode_emit_byte(OP_POP);

    jml_loop_end();
}


static void
jml_break_statement(void)
{
    if (current->loop == NULL) {
        jml_parser_error("Can't use 'break' outside of a loop.");
        return;
    }

    /*placeholder*/
    jml_bytecode_emit_jump(UINT8_MAX);
}


static void
jml_skip_statement(void)
{
    if (current->loop == NULL) {
        jml_parser_error("Can't use 'skip' outside of a loop.");
        return;
    }

    jml_bytecode_emit_loop(current->loop->start);
}


static void
jml_for_statement(void)
{
    jml_scope_begin();

    jml_parser_consume(TOKEN_LPAREN, "Expect '(' after 'for'.");
    if (jml_parser_match(TOKEN_SEMI)) {
        /*w/o initilizer*/
    } else if (jml_parser_match(TOKEN_LET)) {
        jml_let_declaration();
    } else {
        jml_expression_statement();
    }

    int start = jml_bytecode_current()->count;

    int exit = -1;
    if (!jml_parser_match(TOKEN_SEMI)) {
        jml_expression();
        jml_parser_consume(TOKEN_SEMI, "Expect ';' after loop condition.");

        exit = jml_bytecode_emit_jump(OP_JUMP_IF_FALSE);
        jml_bytecode_emit_byte(OP_POP);
    }

    if (!jml_parser_match(TOKEN_RPAREN)) {
        int body = jml_bytecode_emit_jump(OP_JUMP);

        int increment = jml_bytecode_current()->count;
        jml_expression();
        jml_bytecode_emit_byte(OP_POP);
        jml_parser_consume(TOKEN_RPAREN, "Expect ')' after for clauses.");

        jml_bytecode_emit_loop(start);
        start = increment;
        jml_bytecode_patch_jump(body);
    }

    jml_loop_begin(start, jml_bytecode_current()->count, exit);
    jml_statement();

    jml_bytecode_emit_loop(start);
    if (exit != -1) {
        jml_bytecode_patch_jump(exit);
        jml_bytecode_emit_byte(OP_POP);
    }

    jml_loop_end();
    jml_scope_end();
}


/*TODO*/
// static void
// jml_for_statement(void)
// {
//     jml_scope_begin();

//     int start = jml_bytecode_current()->count;

//     jml_parser_consume(TOKEN_LET, "Expect 'let' after 'for'.");
//     uint8_t local = jml_variable_parse("Expect variable name.");
//     jml_variable_definition(local);

//     jml_parser_consume(TOKEN_IN, "Expect 'in' after 'for let'.");
//     //array

//     int exit = jml_bytecode_emit_jump(OP_JUMP_IF_FALSE);

//     jml_bytecode_emit_byte(OP_POP);
//     jml_statement();

//     jml_bytecode_emit_loop(start);

//     jml_bytecode_patch_jump(exit);
//     jml_bytecode_emit_byte(OP_POP);

//     jml_scope_end();
// }


static void
jml_if_statement(void)
{
    jml_parser_match_line();
    jml_expression();
    jml_parser_match_line();

    int then_jump = jml_bytecode_emit_jump(OP_JUMP_IF_FALSE);
    jml_bytecode_emit_byte(OP_POP);

    jml_parser_consume(TOKEN_LBRACE, "Expect '{' after 'if'.");

    jml_block();

    int else_jump = jml_bytecode_emit_jump(OP_JUMP);
    jml_bytecode_patch_jump(then_jump);
    jml_bytecode_emit_byte(OP_POP);

    if (jml_parser_match(TOKEN_ELSE)) {
        jml_parser_match_line();

        jml_parser_consume(TOKEN_LBRACE, "Expect '{' after 'if'.");
        jml_block();
        jml_parser_newline("Expect newline after 'else' block.");

    } else
        jml_parser_newline("Expect newline after 'if' block.");

    jml_bytecode_patch_jump(else_jump);
}


static void
jml_statement(void)
{
    jml_parser_match_line();

    if (jml_parser_match(TOKEN_RETURN))
        jml_return_statement();

    else if (jml_parser_match(TOKEN_IMPORT))
        jml_import_statement();

    else if (jml_parser_match(TOKEN_WHILE))
        jml_while_statement();

    else if (jml_parser_match(TOKEN_BREAK))
        jml_break_statement();

    else if (jml_parser_match(TOKEN_SKIP))
        jml_skip_statement();

    else if (jml_parser_match(TOKEN_FOR))
        jml_for_statement();

    else if (jml_parser_match(TOKEN_IF))
        jml_if_statement();

    else if (jml_parser_match(TOKEN_LBRACE)) {
        jml_scope_begin();
        jml_block();
        jml_scope_end();
        jml_parser_newline("Expect newline after block");

    } else
        jml_expression_statement();

    jml_parser_match_line();
}


jml_obj_function_t *
jml_compiler_compile(const char *source,
    jml_obj_module_t *module, bool output)
{
    jml_lexer_init(&parser.lexer, source);
    jml_compiler_t compiler;
    jml_compiler_init(&compiler, FUNCTION_MAIN, module, output);

    parser.w_error = false;
    parser.panicked = false;

    jml_parser_advance();
    jml_parser_match_line();

    while (!jml_parser_match(TOKEN_EOF)) {
        jml_declaration();
    }

    jml_obj_function_t *function = jml_compiler_end();
    return parser.w_error ? NULL : function;
}


void
jml_compiler_mark_roots(void)
{
    jml_compiler_t *compiler = current;
    while (compiler != NULL) {
        jml_gc_mark_obj((jml_obj_t*)compiler->function);
        compiler = compiler->enclosing;
    }
}
