#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jml_compiler.h>
#include <jml_gc.h>


jml_parser_t parser;

jml_compiler_t *current = NULL;

jml_class_compiler_t *class_current = NULL;

jml_bytecode_t *compiled;


static jml_bytecode_t *
jml_bytecode_current(void)
{
    return &current->function->bytecode;
}


static inline void
jml_mark_initialized(void)
{
    current->locals[current->local_count - 1].depth = current->scope_depth;
}


static jml_token_t
jml_token_emit_synthetic(const char *text)
{
    jml_token_t token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
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
    jml_parser_error_at(&parser.previous, message);
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

#ifdef JML_PRINT_TOKEN
        jml_token_type_print(parser.current.type);
#endif
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


static void
jml_parser_consume_double(jml_token_type type1,
    jml_token_type type2, const char *message)
{
    if (parser.current.type == type1
    || parser.current.type == type2) {
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
    if (current->type == FUNCTION_INITIALIZER) {
        jml_bytecode_emit_bytes(OP_GET_LOCAL, 0);
    } else {
        jml_bytecode_emit_byte(OP_NONE);
    }

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

    jml_local_t *local = &current->locals[current->local_count++];
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


/*forwarded declaration*/
static void jml_expression();

static void jml_statement();

static void jml_declaration();

static jml_parser_rule *jml_parser_rule_get(
    jml_token_type type);

static void jml_parser_precedence_parse(
    jml_parser_precedence precedence);


static void
jml_parser_synchronize(void)
{
    parser.panicked = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMI
        || (parser.previous.type == TOKEN_LINE))
            return;

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

            default: /*pass*/;
        }
        jml_parser_advance();
    }
}


static inline uint8_t
jml_identifier_const(jml_token_t *name) {
    return jml_bytecode_make_const(
        OBJ_VAL(jml_obj_string_copy(name->start, name->length))
    );
}


static bool
jml_identifier_equal(jml_token_t *a,
    jml_token_t *b) 
{
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}


static int
jml_local_resolve(jml_compiler_t *compiler, jml_token_t *name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        jml_local_t *local = &compiler->locals[i];
        if (jml_identifier_equal(name, &local->name)) {
        if (local->depth == -1) {
            jml_parser_error("Can't read local variable in its own initializer.");
        }
        return i;
        }
    }

    return -1;
}


static void
jml_local_add(jml_token_t name) {
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
        jml_parser_error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalue_count].local = local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->function->upvalue_count++;
}


static int
jml_upvalue_resolve(jml_compiler_t *compiler, jml_token_t *name) {
    if (compiler->enclosing == NULL) return -1;

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
jml_variable_declaration(void)
{
    if (current->scope_depth == 0) return;

    jml_token_t *name = &parser.previous;

    for (int i = current->local_count - 1; i >= 0; i--) {
        jml_local_t *local = &current->locals[i];
        if (local->depth != -1
            && local->depth < current->scope_depth) {
            break;
        }
        
        if (jml_identifier_equal(name, &local->name)) {
            jml_parser_error(
                "Already variable with this name in this scope."
            );
        }
    }

    jml_local_add(*name);
}


static uint8_t
jml_variable_parse(const char *message) 
{
    jml_parser_consume(TOKEN_NAME, message);

    jml_variable_declaration(); /*global declaration*/
    if (current->scope_depth > 0) return 0;

    return jml_identifier_const(&parser.previous);
}


static void
jml_variable_definition(uint8_t global)
{
    if (current->scope_depth > 0) {
        jml_mark_initialized();
        return;
    }

    jml_bytecode_emit_bytes(OP_DEFINE_GLOBAL, global);
}


static uint8_t
jml_arguments_list(void)
{
    uint8_t arg_count = 0;
    if (!jml_parser_check(TOKEN_RPAREN)) {
        do {
            jml_expression();
            if (arg_count == 255)
                jml_parser_error("Can't have more than 255 arguments.");

            arg_count++;
        } while (jml_parser_match(TOKEN_COMMA));
    }

    jml_parser_consume(TOKEN_RPAREN, "Expect ')' after arguments.");
    return arg_count;
}


static void
jml_and(bool assignable)
{
  int jump_end = jml_bytecode_emit_jump(OP_JMP_IF_FALSE);

  jml_bytecode_emit_byte(OP_POP);
  jml_parser_precedence_parse(PREC_AND);

  jml_bytecode_patch_jump(jump_end);
}


static void
jml_or(bool assignable)
{
    int jump_else = jml_bytecode_emit_jump(OP_JMP_IF_FALSE);
    int jump_end = jml_bytecode_emit_jump(OP_JMP);

    jml_bytecode_patch_jump(jump_else);
    jml_bytecode_emit_byte(OP_POP);

    jml_parser_precedence_parse(PREC_OR);
    jml_bytecode_patch_jump(jump_end);
}


static void
jml_binary(bool assignable)
{

    jml_token_type operatorType = parser.previous.type;

    jml_parser_rule *rule = jml_parser_rule_get(operatorType);
    jml_parser_precedence_parse(
        (jml_parser_precedence)(rule->precedence + 1)
    );

    switch (operatorType) {
        case TOKEN_NOTEQ:           jml_bytecode_emit_byte(OP_NOTEQ);       break;
        case TOKEN_EQEQUAL:         jml_bytecode_emit_byte(OP_EQUAL);       break;
        case TOKEN_GREATER:         jml_bytecode_emit_byte(OP_GREATER);     break;
        case TOKEN_GREATEREQ:       jml_bytecode_emit_byte(OP_LESSEQ);      break;
        case TOKEN_LESS:            jml_bytecode_emit_byte(OP_LESS);        break;
        case TOKEN_LESSEQ:          jml_bytecode_emit_byte(OP_GREATEREQ);   break;
        case TOKEN_PLUS:            jml_bytecode_emit_byte(OP_ADD);         break;
        case TOKEN_MINUS:           jml_bytecode_emit_byte(OP_SUB);         break;
        case TOKEN_STAR:            jml_bytecode_emit_byte(OP_MUL);         break;
        case TOKEN_SLASH:           jml_bytecode_emit_byte(OP_DIV);         break;
        case TOKEN_STARSTAR:        jml_bytecode_emit_byte(OP_POW);         break;
        case TOKEN_PERCENT:         jml_bytecode_emit_byte(OP_MOD);         break;

        default:
            UNREACHABLE();
            return;
    }
}


static void
jml_call(bool assignable)
{
    uint8_t arg_count = jml_arguments_list();
    jml_bytecode_emit_bytes(OP_CALL, arg_count);
}


static void
jml_dot(bool assignable)
{
    jml_parser_consume(TOKEN_NAME, "Expect property name after '.'.");
    uint8_t name = jml_identifier_const(&parser.previous);

    if (assignable && jml_parser_match(TOKEN_EQUAL)) {
        jml_expression();
        jml_bytecode_emit_bytes(OP_SET_PROPERTY, name);
    } else if (jml_parser_match(TOKEN_LPAREN)) {
        uint8_t arg_count = jml_arguments_list();
        jml_bytecode_emit_bytes(OP_INVOKE, name);
        jml_bytecode_emit_byte(arg_count);
    } else {
        jml_bytecode_emit_bytes(OP_GET_PROPERTY, name);
    }
}


static void
jml_literal(bool assignable)
{
    switch (parser.previous.type) {
        case TOKEN_FALSE:   jml_bytecode_emit_byte(OP_FALSE);       break;
        case TOKEN_NONE:    jml_bytecode_emit_byte(OP_NONE);        break;
        case TOKEN_TRUE:    jml_bytecode_emit_byte(OP_TRUE);        break;

        default:            UNREACHABLE();  return;
    }
}


static void
jml_grouping(bool assignable)
{
    jml_expression();
    jml_parser_consume(TOKEN_RPAREN, "Expect ')' after expression.");
}


static void
jml_number(bool assignable)
{
    double value = strtod(parser.previous.start, NULL);
    jml_bytecode_emit_const(NUMBER_VAL(value));
}


static void
jml_string(bool assignable)
{
    jml_bytecode_emit_const(
        OBJ_VAL(jml_obj_string_copy(parser.previous.start + 1,
            parser.previous.length - 2))
    );
}


static void
jml_variable_named(jml_token_t name,
    bool assignable)
{
    uint8_t get_op, set_op;
    int arg = jml_local_resolve(current, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else if ((arg = jml_upvalue_resolve(current,&name)) != -1) {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    } else {
        arg = jml_identifier_const(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (assignable && jml_parser_match(TOKEN_EQUAL)) {
        jml_expression();
        jml_bytecode_emit_bytes(set_op, (uint8_t)arg);
    } else {
        jml_bytecode_emit_bytes(get_op, (uint8_t)arg);
    }
}


static void jml_variable(bool assignable) {
    jml_variable_named(parser.previous, assignable);
}


static void
jml_super(bool assignable)
{
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
        jml_bytecode_emit_bytes(OP_GET_SUPER, name);
    }
}


static void
jml_self(bool assignable)
{
    if (class_current == NULL) {
        jml_parser_error("Can't use 'self' outside of a class.");
        return;
    }
    jml_variable(false);
}


static void
jml_unary(bool assignable)
{
    jml_token_type type = parser.previous.type;
    jml_parser_precedence_parse(PREC_UNARY);

    switch (type) {
        case TOKEN_NOT:         jml_bytecode_emit_byte(OP_NOT);       break;
        case TOKEN_MINUS:       jml_bytecode_emit_byte(OP_NEGATE);    break;

        default:                UNREACHABLE(); return;
    }
}


jml_parser_rule rules[] = {
    [TOKEN_RPAREN]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LPAREN]      = {jml_grouping, jml_call,  PREC_CALL},
    [TOKEN_RSQARE]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LSQARE]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RBRACE]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LBRACE]      = {NULL,        NULL,       PREC_NONE},

    [TOKEN_COLON]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SEMI]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_COMMA]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_DOT]         = {NULL,        jml_dot,    PREC_CALL},

    [TOKEN_PIPE]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_CARET]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_AMP]         = {NULL,        NULL,       PREC_NONE},
    [TOKEN_TILDE]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_QUEST]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_BANG]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_HASH]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_AT]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ARROW]       = {NULL,        NULL,       PREC_NONE},

    [TOKEN_PLUS]        = {NULL,        jml_binary, PREC_TERM},
    [TOKEN_MINUS]       = {jml_unary,   jml_binary, PREC_TERM},
    [TOKEN_STAR]        = {NULL,        jml_binary, PREC_FACTOR},
    [TOKEN_STARSTAR]    = {NULL,        jml_binary, PREC_FACTOR},
    [TOKEN_SLASH]       = {NULL,        jml_binary, PREC_FACTOR},
    [TOKEN_PERCENT]     = {NULL,        jml_binary, PREC_FACTOR},

    [TOKEN_EQUAL]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EQEQUAL]     = {NULL,        jml_binary, PREC_EQUALITY},
    [TOKEN_GREATER]     = {NULL,        jml_binary, PREC_COMPARISON},
    [TOKEN_GREATEREQ]   = {NULL,        jml_binary, PREC_COMPARISON},
    [TOKEN_LESS]        = {NULL,        jml_binary, PREC_COMPARISON},
    [TOKEN_LESSEQ]      = {NULL,        jml_binary, PREC_COMPARISON},
    [TOKEN_NOTEQ]       = {NULL,        jml_binary, PREC_EQUALITY},

    [TOKEN_FOR]         = {NULL,        NULL,       PREC_NONE},
    [TOKEN_WHILE]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_BREAK]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SKIP]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IN]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_WITH]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IF]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ELSE]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_CLASS]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SELF]        = {jml_super,   NULL,       PREC_NONE},
    [TOKEN_SUPER]       = {jml_self,    NULL,       PREC_NONE},
    [TOKEN_LET]         = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FN]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RETURN]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IMPORT]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_AND]         = {NULL,        jml_and,    PREC_AND},
    [TOKEN_NOT]         = {jml_unary,   NULL,       PREC_NONE},
    [TOKEN_OR]          = {NULL,        jml_or,     PREC_OR},

    /*TODO*/
    [TOKEN_ATOM]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_TRUE]        = {jml_literal, NULL,       PREC_NONE},
    [TOKEN_FALSE]       = {jml_literal, NULL,       PREC_NONE},
    [TOKEN_NONE]        = {jml_literal, NULL,       PREC_NONE},

    [TOKEN_NAME]        = {jml_variable,NULL,       PREC_NONE},
    [TOKEN_NUMBER]      = {jml_number,  NULL,       PREC_NONE},
    [TOKEN_STRING]      = {jml_string,  NULL,       PREC_NONE},

    [TOKEN_LINE]        = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ERROR]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EOF]         = {NULL,        NULL,       PREC_NONE}
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
    jml_parser_fn prefix_rule = jml_parser_rule_get(parser.previous.type)->prefix;

    if (prefix_rule == NULL) {
        jml_parser_error("Expect expression.");
        return;
    }

    bool assignable = precedence <= PREC_ASSIGNMENT;
    prefix_rule(assignable);

    while (precedence <=
        jml_parser_rule_get(parser.current.type)->precedence) {

        jml_parser_advance();
        jml_parser_fn infix_rule = jml_parser_rule_get(parser.previous.type)->infix;
        infix_rule(assignable);
    }

    if (assignable && jml_parser_match(TOKEN_EQUAL)) {
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
    while (!jml_parser_check(TOKEN_LBRACE)
        && !jml_parser_check(TOKEN_EOF)) {

        jml_declaration();
    }

    jml_parser_consume(TOKEN_RBRACE, "Expect '}' after block.");
}


static void
jml_function(jml_function_type type)
{
    jml_compiler_t compiler;
    jml_compiler_init(&compiler, type);
    jml_scope_begin();

    jml_parser_consume(TOKEN_LPAREN, "Expect '(' after function name.");
    if (!jml_parser_check(TOKEN_RPAREN)) {
        do {
        current->function->arity++;
        if (current->function->arity > 255) {
            jml_parser_error_current("Can't have more than 255 parameters.");
        }

        uint8_t param_const = jml_variable_parse("Expect parameter name.");
        jml_variable_definition(param_const);
        } while (jml_parser_match(TOKEN_COMMA));
    }

    jml_parser_consume(TOKEN_RPAREN, "Expect ')' after parameters.");
    jml_parser_consume(TOKEN_LBRACE, "Expect '{' before function body.");
    jml_block();

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
    jml_parser_consume(TOKEN_NAME, "Expect method name.");
    uint8_t constant = jml_identifier_const(&parser.previous);

    jml_function_type type = FUNCTION_METHOD;
    if (parser.previous.length == 4 &&
        memcmp(parser.previous.start, "init", 4) == 0) {
        type = FUNCTION_INITIALIZER;
    }
    jml_function(type);
    jml_bytecode_emit_bytes(OP_METHOD, constant);
}


static void
jml_class_declaration(void)
{
    jml_parser_consume(TOKEN_NAME, "Expect class name.");
    jml_token_t class_name = parser.previous;
    uint8_t name_const = jml_identifier_const(&parser.previous);
    jml_variable_declaration(); /*global declaration*/

    jml_bytecode_emit_bytes(OP_CLASS, name_const);
    jml_variable_definition(name_const);

    jml_class_compiler_t class_compiler;
    class_compiler.name = parser.previous;
    class_compiler.enclosing = class_current;
    class_compiler.w_superclass = false;
    class_current = &class_compiler;

    if (jml_parser_match(TOKEN_LESS)) {
        jml_parser_consume(TOKEN_NAME, "Expect superclass name.");
        jml_variable(false);

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

    while (!jml_parser_check(TOKEN_RBRACE)
        && !jml_parser_check(TOKEN_EOF)) {
        jml_method();
    }

    jml_parser_consume(TOKEN_RBRACE, "Expect '}' after class body.");
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
    jml_mark_initialized();
    jml_function(FUNCTION_FN);
    jml_variable_definition(global);
}


static void
jml_let_declaration(void)
{
    uint8_t global = jml_variable_parse("Expect variable name.");

    if (jml_parser_match(TOKEN_EQUAL)) {
        jml_expression();
    } else {
        jml_bytecode_emit_byte(OP_NONE);
    }
    jml_parser_consume(
        TOKEN_SEMI, "Expect ';' after variable declaration."
    );

    jml_variable_definition(global);
}


static void
jml_expression_statement(void)
{
    jml_expression();
    jml_parser_consume_double(
        TOKEN_SEMI, TOKEN_LINE, "Expect newline or ';' after expression."
    );
    jml_bytecode_emit_byte(OP_POP);
}


static void
jml_if_statement(void)
{
    jml_parser_consume(TOKEN_LPAREN, "Expect '(' after 'if'.");
    jml_expression();
    jml_parser_consume(TOKEN_RPAREN, "Expect ')' after condition.");

    int then_jump = jml_bytecode_emit_jump(OP_JMP_IF_FALSE);
    jml_bytecode_emit_byte(OP_POP);
    jml_statement();

    int else_jump = jml_bytecode_emit_jump(OP_JMP);
    jml_bytecode_patch_jump(then_jump);
    jml_bytecode_emit_byte(OP_POP);

    if (jml_parser_match(TOKEN_ELSE)) jml_statement();
    jml_bytecode_patch_jump(else_jump);
}


static void
jml_return_statement(void)
{
    if (current->type == FUNCTION_MAIN) {
        jml_parser_error("Can't return from top-level code.");
    }

    if (jml_parser_match(TOKEN_SEMI)) {
        jml_bytecode_emit_return();
    } else {
        if (current->type == FUNCTION_INITIALIZER)
            jml_parser_error("Can't return a value from an initializer.");

        jml_expression();
        jml_parser_consume_double(
            TOKEN_SEMI, TOKEN_LINE, "Expect newline or ';' after return value."
        );
        jml_bytecode_emit_byte(OP_RETURN);
    }
}


static void
jml_while_statement(void)
{
    int start = jml_bytecode_current()->count;

    jml_parser_consume(TOKEN_LPAREN, "Expect '(' after 'while'.");
    jml_expression();
    jml_parser_consume(TOKEN_RPAREN, "Expect ')' after condition.");

    int exit = jml_bytecode_emit_jump(OP_JMP_IF_FALSE);

    jml_bytecode_emit_byte(OP_POP);
    jml_statement();

    jml_bytecode_emit_loop(start);

    jml_bytecode_patch_jump(exit);
    jml_bytecode_emit_byte(OP_POP);
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

        exit = jml_bytecode_emit_jump(OP_JMP_IF_FALSE);
        jml_bytecode_emit_byte(OP_POP);
    }

    if (!jml_parser_match(TOKEN_RPAREN)) {
        int body = jml_bytecode_emit_jump(OP_JMP);

        int incrementStart = jml_bytecode_current()->count;
        jml_expression();
        jml_bytecode_emit_byte(OP_POP);
        jml_parser_consume(TOKEN_RPAREN, "Expect ')' after for clauses.");

        jml_bytecode_emit_loop(start);
        start = incrementStart;
        jml_bytecode_patch_jump(body);
    }

    jml_statement();

    jml_bytecode_emit_loop(start);
    if (exit != -1) {
        jml_bytecode_patch_jump(exit);
        jml_bytecode_emit_byte(OP_POP);
    }

    jml_scope_end();
}


static void
jml_declaration(void)
{
    if (jml_parser_match(TOKEN_CLASS)) {
        jml_class_declaration();
    } else if (jml_parser_match(TOKEN_FN)) {
        jml_function_declaration();
    } else if (jml_parser_match(TOKEN_LET)) {
        jml_let_declaration();
    } else {
        jml_statement();
    }

    if (parser.panicked)
        jml_parser_synchronize();
}


static void
jml_statement(void)
{
    if (jml_parser_match(TOKEN_RETURN)) {
        jml_return_statement();
    } else if (jml_parser_match(TOKEN_FOR)) {
        jml_for_statement();
    } else if (jml_parser_match(TOKEN_IF)) {
        jml_if_statement();
    } else if (jml_parser_match(TOKEN_WHILE)) {
        jml_while_statement();
    } else if (jml_parser_match(TOKEN_LBRACE)) {
        jml_scope_begin();
        jml_block();
        jml_scope_end();
    } else {
        jml_expression();
    }
}


jml_obj_function_t *
jml_compiler_compile(const char *source)
{
    jml_lexer_init(source);
    jml_compiler_t compiler;
    jml_compiler_init(&compiler, FUNCTION_MAIN);

    parser.w_error = false;
    parser.panicked = false;

    jml_parser_advance();
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
