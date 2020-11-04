#include <string.h>

#include <jml_lexer.h>


jml_lexer_t lexer;


void
jml_lexer_init(const char *source) {
    lexer.start   = source;
    lexer.current = source;
    lexer.line    = 0;
}


static inline bool
jml_is_eol(void)
{
    return *lexer.current == '\0';
}


static inline char
jml_peek_char(void)
{
    return *lexer.current;
}


static inline char
jml_peek_next_char(void)
{
    if (jml_is_eol()) return '\0';
    return lexer.current[1];
}


static char
jml_advance(void)
{
    lexer.current++;
    return lexer.current[-1];
}


static bool
jml_match(char expected)
{
    if (jml_is_eol())               return false;
    if (*lexer.current != expected) return false;

    lexer.current++;
    return true;
}


static jml_token_t
jml_emit_token(jml_token_type_t type)
{
    jml_token_t token;
    token.type   = type;
    token.start  = lexer.start;
    token.length = lexer.current - lexer.start;
    token.line   = lexer.line;

    return token;
}


static jml_token_t
jml_emit_error_token(const char *message)
{
    jml_token_t token;
    token.type   = TOKEN_ERROR;
    token.start  = lexer.start;
    token.length = strlen(message);
    token.line   = lexer.line;

    return token;
}


static void
jml_skip_char(void)
{
    while (true) {
        char c = jml_peek_char();
        switch (c) {
            case  ' ':
            case '\t':
            case '\r':
                jml_advance();
                break;

            case  '?':
                while (jml_peek_char() != '\n' && !jml_is_eol()) jml_advance();
                break;

            default: return;
        }
    }
}


static jml_token_type_t
jml_check_keyword(int start, size_t length,
    const char *rest, jml_token_type_t type)
{
    if ((lexer.current - lexer.start) == (length + start)
        && memcmp(lexer.start + start, rest, length)) {
        return type;
    }

    return TOKEN_NAME;
}


static jml_token_type_t
jml_check_identifier(void)
{
    switch (lexer.start[0]) {
        case 'a': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'n': return jml_check_keyword(2, 1, "d", TOKEN_AND);
                    case 't': return jml_check_keyword(2, 2, "om", TOKEN_ATOM);
                }
            }
            break;
        case 'b': return jml_check_keyword(1, 4, "reak", TOKEN_BREAK);
        case 'c': return jml_check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return jml_check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'a': return jml_check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'n': return jml_check_keyword(2, 0, "", TOKEN_FUNC);
                    case 'o': return jml_check_keyword(2, 1, "r", TOKEN_FOR);
                }
            }
            break;
        case 'i':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'f': return jml_check_keyword(2, 0, "", TOKEN_IF);
                    case 'n': return jml_check_keyword(2, 0, "", TOKEN_IN);
                    case 'm': return jml_check_keyword(2, 4, "port", TOKEN_IMPORT);
                }
            }
            break;
        case 'n': 
            if (lexer.current - lexer.start > 1
                && lexer.start[1] == 'o') {
                switch (lexer.start[2]) {
                    case 'n': return jml_check_keyword(3, 1, "e", TOKEN_NONE);
                    case 't': return jml_check_keyword(3, 0, "", TOKEN_NOT);
                }
            }
            break;
        case 'o': return jml_check_keyword(1, 1, "r", TOKEN_OR);
        case 'l': return jml_check_keyword(1, 2, "et", TOKEN_LET);
        case 'r': return jml_check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'e': return jml_check_keyword(2, 2, "lf", TOKEN_SELF);
                    case 'k': return jml_check_keyword(2, 2, "ip", TOKEN_SKIP);
                    case 'u': return jml_check_keyword(2, 3, "per", TOKEN_SUPER);
                }
            }
            break;
        case 't': return jml_check_keyword(1, 3, "rue", TOKEN_TRUE);
        case 'w': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'i': return jml_check_keyword(2, 2, "th", TOKEN_WITH);
                    case 'h': return jml_check_keyword(2, 3, "ile", TOKEN_WHILE);
                }
            }
            break;
    }
    return TOKEN_NAME;
}


static jml_token_t
jml_identifier_literal(void)
{
  while (jml_is_alpha(jml_peek_char())
        || jml_is_digit(jml_peek_char())) jml_advance();

  return jml_emit_token(jml_check_identifier());
}


static jml_token_t
jml_string_literal(const char delimiter)
{
    while (jml_peek_char() != delimiter && !jml_is_eol()) {
        char c =        jml_peek_char();
        if  (c == '\n') lexer.line++;
        jml_advance();
    }

    if (jml_is_eol())
        return jml_emit_error_token("Unterminated string.");

    jml_advance();
    return jml_emit_token(TOKEN_STRING);
}


static jml_token_t
jml_number_literal(void)
{
    while (jml_is_digit(jml_peek_char())) jml_advance();

    if (jml_peek_char() == '.' && jml_is_digit(jml_peek_next_char())) {
        jml_advance();
        while (jml_is_digit(jml_peek_char())) jml_advance();
    }

    return jml_emit_token(TOKEN_NUMBER);
}


jml_token_t
jml_lexer_tokenize(void)
{
    jml_skip_char();

    lexer.start =           lexer.current;
    if (jml_is_eol())       return jml_emit_token(TOKEN_EOF);

    char c      =           jml_advance();
    if (jml_is_alpha(c))    return jml_identifier_literal();
    if (jml_is_digit(c))    return jml_number_literal();

    switch (c) {
        case  '(':  return jml_emit_token(TOKEN_RPAREN);
        case  ')':  return jml_emit_token(TOKEN_LPAREN);
        case  '[':  return jml_emit_token(TOKEN_RSQARE);
        case  ']':  return jml_emit_token(TOKEN_LSQARE);
        case  '{':  return jml_emit_token(TOKEN_RBRACE);
        case  '}':  return jml_emit_token(TOKEN_LBRACE);

        case  ':':  return jml_emit_token(TOKEN_COLON);
        case  ';':  return jml_emit_token(TOKEN_SEMI);
        case  ',':  return jml_emit_token(TOKEN_COMMA);
        case  '.':  return jml_emit_token(TOKEN_DOT);

        case  '+':  return jml_emit_token(TOKEN_PLUS);
        case  '*':
            return  jml_emit_token(jml_match('*')
                    ? TOKEN_STARSTAR : TOKEN_STAR);
        case  '/':  return jml_emit_token(TOKEN_SLASH);
        case  '%':  return jml_emit_token(TOKEN_PERCENT);
        case  '-':
            return  jml_emit_token(jml_match('>')
                    ? TOKEN_ARROW : TOKEN_MINUS);

        case  '=':
            return  jml_emit_token(jml_match('=')
                    ? TOKEN_EQEQUAL : TOKEN_EQUAL);
        case  '<':
            return  jml_emit_token(jml_match('=')
                    ? TOKEN_LESSEQ : TOKEN_LESS);
        case  '>':
            return  jml_emit_token(jml_match('=')
                    ? TOKEN_GREATEREQ : TOKEN_GREATER);

        case '\'':  return jml_string_literal('\'');
        case  '"':  return jml_string_literal('"');

        case  '@':  return jml_emit_token(TOKEN_AT);
        case  '|':  return jml_emit_token(TOKEN_PIPE);
        case  '~':  return jml_emit_token(TOKEN_TILDE);
        case  '&':  return jml_emit_token(TOKEN_AMP);
        case  '^':  return jml_emit_token(TOKEN_CARET);
        case  '?':  return jml_emit_token(TOKEN_QUEST);
        case  '#':  return jml_emit_token(TOKEN_HASH);
        case  '!':  return jml_emit_token(TOKEN_BANG);

        case '\n':
            lexer.line++;
            return jml_emit_token(TOKEN_LINE);
    }
    return jml_emit_error_token("Unexpected character.");
}
