#include <string.h>
#include <stdio.h>

#include <jml_lexer.h>
#include <jml_util.h>


jml_lexer_t lexer;


void
jml_lexer_init(const char *source) {
    lexer.start     = source;
    lexer.current   = source;
    lexer.line      = 1;
    lexer.eof       = false;
}


static inline bool
jml_is_eof(void)
{
    return *lexer.current == '\0';
}


static inline char
jml_lexer_peek(void)
{
    return *lexer.current;
}


static inline char
jml_lexer_peek_next(void)
{
    if (jml_is_eof()) return '\0';
    return lexer.current[1];
}


static inline char
jml_lexer_peek_previous(void)
{
    return lexer.current[-1];
}


static inline char
jml_lexer_advance(void)
{
    lexer.current++;
    return lexer.current[-1];
}


static inline void
jml_lexer_newline(void)
{
    lexer.line++;
}


static bool
jml_lexer_match(char expected)
{
    if (jml_is_eof())               return false;
    if (*lexer.current != expected) return false;

    lexer.current++;
    return true;
}


static jml_token_t
jml_token_emit(jml_token_type type)
{
    jml_token_t token;
    token.type   = type;
    token.start  = lexer.start;
    token.length = lexer.current - lexer.start;
    token.line   = lexer.line;

    return token;
}


static jml_token_t
jml_token_emit_error(const char *message)
{
    jml_token_t token;
    token.type   = TOKEN_ERROR;
    token.start  = message;
    token.length = strlen(message);
    token.line   = lexer.line;

    return token;
}


static void
jml_lexer_skip_char(void)
{
    static bool commented = false;

    while (true) {
        char c = jml_lexer_peek();

        if (commented && c != '?' && c != '\n') {
            if (jml_is_eof()) {
                commented = !commented;
                return;
            }

            jml_lexer_advance();
            continue;
        }

        switch (c) {
            case  ' ':
            case '\t':
            case '\r':
                jml_lexer_advance();
                break;

            case '\n':
                jml_lexer_newline();
                if (commented) {
                    jml_lexer_advance();
                    break;
                } else
                    return;

            case '\\':
                jml_lexer_advance();
                break;

            case  '!':
                if (jml_lexer_peek_next() == '=') return;
                while (jml_lexer_peek() != '\n' && !jml_is_eof()) jml_lexer_advance();

                if (jml_lexer_peek() == '\n') {
                    jml_lexer_newline();
                    jml_lexer_advance();
                }
                break;

            case '?':
                commented = !commented;
                jml_lexer_advance();
                break;

            default: return;
        }
    }
}


static jml_token_type
jml_keyword_match(int start, int length,
    const char *rest, jml_token_type type)
{
    if ((lexer.current - lexer.start) == (length + start)
        && memcmp(lexer.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_NAME;
}


static jml_token_type
jml_identifier_check(void)
{
    switch (lexer.start[0]) {
        case 'a': return jml_keyword_match(1, 2, "nd", TOKEN_AND);
        case 'b': return jml_keyword_match(1, 4, "reak", TOKEN_BREAK);
        case 'c': return jml_keyword_match(1, 4, "lass", TOKEN_CLASS);
        case 'e': return jml_keyword_match(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'a': return jml_keyword_match(2, 3, "lse", TOKEN_FALSE);
                    case 'n': return jml_keyword_match(2, 0, "", TOKEN_FN);
                    case 'o': return jml_keyword_match(2, 1, "r", TOKEN_FOR);
                }
            }
            break;
        case 'i':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'f': return jml_keyword_match(2, 0, "", TOKEN_IF);
                    case 'n': return jml_keyword_match(2, 0, "", TOKEN_IN);
                    case 'm': return jml_keyword_match(2, 4, "port", TOKEN_IMPORT);
                }
            }
            break;
        case 'n':
            if (lexer.current - lexer.start > 1
                && lexer.start[1] == 'o') {
                switch (lexer.start[2]) {
                    case 'n': return jml_keyword_match(3, 1, "e", TOKEN_NONE);
                    case 't': return jml_keyword_match(3, 0, "", TOKEN_NOT);
                }
            }
            break;
        case 'o': return jml_keyword_match(1, 1, "r", TOKEN_OR);
        case 'l': return jml_keyword_match(1, 2, "et", TOKEN_LET);
        case 'r': return jml_keyword_match(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'e': return jml_keyword_match(2, 2, "lf", TOKEN_SELF);
                    case 'k': return jml_keyword_match(2, 2, "ip", TOKEN_SKIP);
                    case 'u': return jml_keyword_match(2, 3, "per", TOKEN_SUPER);
                }
            }
            break;
        case 't': return jml_keyword_match(1, 3, "rue", TOKEN_TRUE);
        case 'w':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'i': return jml_keyword_match(2, 2, "th", TOKEN_WITH);
                    case 'h': return jml_keyword_match(2, 3, "ile", TOKEN_WHILE);
                }
            }
            break;
    }
    return TOKEN_NAME;
}


static jml_token_t
jml_identifier_literal(void)
{
    while (jml_is_alpha(jml_lexer_peek())
        || jml_is_digit(jml_lexer_peek()))

        jml_lexer_advance();

    return jml_token_emit(jml_identifier_check());
}


static jml_token_t
jml_string_literal(const char delimiter)
{
    while (jml_lexer_peek() != delimiter) {
        char c =        jml_lexer_peek();
        if  (c == '\n') jml_lexer_newline();

        if  (jml_is_eof())
            return jml_token_emit_error("Unterminated string.");

        jml_lexer_advance();
    }

    jml_lexer_advance();
    return jml_token_emit(TOKEN_STRING);
}


static jml_token_t
jml_number_literal(void)
{
    if (jml_lexer_peek_previous() == '0'
        && jml_lexer_peek() == 'x') {

        jml_lexer_advance();
        if (!jml_is_hex(jml_lexer_peek()))
            return jml_token_emit_error("Unterminated hex literal.");

        while (jml_is_hex(jml_lexer_peek())) jml_lexer_advance();

        if ((jml_lexer_peek() == '.')
            && jml_is_hex(jml_lexer_peek_next())) {

            jml_lexer_advance();
            while (jml_is_hex(jml_lexer_peek())) jml_lexer_advance();
        }
    } else {
        while (jml_is_digit(jml_lexer_peek())) jml_lexer_advance();

        if ((jml_lexer_peek() == '.')
            && jml_is_digit(jml_lexer_peek_next())) {

            jml_lexer_advance();
            while (jml_is_digit(jml_lexer_peek())) jml_lexer_advance();
        }
    }

    return jml_token_emit(TOKEN_NUMBER);
}


jml_token_t
jml_lexer_tokenize(void)
{
    if (lexer.eof)          return jml_token_emit(TOKEN_EOF);

    jml_lexer_skip_char();
    lexer.start     =       lexer.current;

    if (jml_is_eof()) {
        lexer.eof   =       true;
        return jml_token_emit(TOKEN_LINE);
    }

    char c          =       jml_lexer_advance();
    if (jml_is_alpha(c))    return jml_identifier_literal();
    if (jml_is_digit(c))    return jml_number_literal();

    switch (c) {
        case  ')':  return  jml_token_emit(TOKEN_RPAREN);
        case  '(':  return  jml_token_emit(TOKEN_LPAREN);
        case  ']':  return  jml_token_emit(TOKEN_RSQARE);
        case  '[':  return  jml_token_emit(TOKEN_LSQARE);
        case  '}':  return  jml_token_emit(TOKEN_RBRACE);
        case  '{':  return  jml_token_emit(TOKEN_LBRACE);

        case  ':':  return  jml_token_emit(TOKEN_COLON);
        case  ';':  return  jml_token_emit(TOKEN_SEMI);
        case  ',':  return  jml_token_emit(TOKEN_COMMA);
        case  '.':  return  jml_token_emit(TOKEN_DOT);

        case  '+':  return  jml_token_emit(TOKEN_PLUS);
        case  '*':  return  jml_token_emit(jml_lexer_match('*')
                        ? TOKEN_STARSTAR : TOKEN_STAR);

        case  '/':  return  jml_token_emit(TOKEN_SLASH);
        case  '%':  return  jml_token_emit(TOKEN_PERCENT);

        case  '-':  return  jml_token_emit(jml_lexer_match('>')
                        ? TOKEN_ARROW : TOKEN_MINUS);

        case  '=':  return  jml_token_emit(jml_lexer_match('=')
                        ? TOKEN_EQEQUAL : TOKEN_EQUAL);

        case  '<':  return  jml_token_emit(jml_lexer_match('=')
                        ? TOKEN_LESSEQ : TOKEN_LESS);

        case  '>':  return  jml_token_emit(jml_lexer_match('=')
                        ? TOKEN_GREATEREQ : TOKEN_GREATER);

        case  '!':  return  jml_token_emit(jml_lexer_match('=')
                        ? TOKEN_NOTEQ : TOKEN_BANG);

        case '\'':  return  jml_string_literal('\'');
        case  '"':  return  jml_string_literal('"');

        case '\n':  return  jml_token_emit(TOKEN_LINE);

        case  '@':  return  jml_token_emit(TOKEN_AT);
        case  '|':  return  jml_token_emit(TOKEN_PIPE);
        case  '~':  return  jml_token_emit(TOKEN_TILDE);
        case  '&':  return  jml_token_emit(TOKEN_AMP);
        case  '^':  return  jml_token_emit(TOKEN_CARET);
        case  '?':  return  jml_token_emit(TOKEN_QUEST);
        case  '#':  return  jml_token_emit(TOKEN_HASH);
    }
    return jml_token_emit_error("Unexpected character.");
}


void
jml_token_type_print(jml_token_type type)
{
    switch(type) {
        case TOKEN_RPAREN:
            printf("%s", "TOKEN_RPAREN");
            break;

        case TOKEN_LPAREN:
            printf("%s", "TOKEN_LPAREN");
            break;

        case TOKEN_RSQARE:
            printf("%s", "TOKEN_RSQARE");
            break;

        case TOKEN_LSQARE:
            printf("%s", "TOKEN_LSQARE");
            break;

        case TOKEN_RBRACE:
            printf("%s", "TOKEN_RBRACE");
            break;

        case TOKEN_LBRACE:
            printf("%s", "TOKEN_LBRACE");
            break;

        case TOKEN_COLON:
            printf("%s", "TOKEN_COLON");
            break;

        case TOKEN_SEMI:
            printf("%s", "TOKEN_SEMI");
            break;

        case TOKEN_COMMA:
            printf("%s", "TOKEN_COMMA");
            break;

        case TOKEN_DOT:
            printf("%s", "TOKEN_DOT");
            break;

        case TOKEN_PIPE:
            printf("%s", "TOKEN_PIPE");
            break;

        case TOKEN_CARET:
            printf("%s", "TOKEN_CARET");
            break;

        case TOKEN_AMP:
            printf("%s", "TOKEN_AMP");
            break;

        case TOKEN_TILDE:
            printf("%s", "TOKEN_TILDE");
            break;

        case TOKEN_QUEST:
            printf("%s", "TOKEN_QUEST");
            break;

        case TOKEN_BANG:
            printf("%s", "TOKEN_BANG");
            break;

        case TOKEN_HASH:
            printf("%s", "TOKEN_HASH");
            break;

        case TOKEN_AT:
            printf("%s", "TOKEN_AT");
            break;

        case TOKEN_ARROW:
            printf("%s", "TOKEN_ARROW");
            break;

        case TOKEN_PLUS:
            printf("%s", "TOKEN_PLUS");
            break;

        case TOKEN_MINUS:
            printf("%s", "TOKEN_MINUS");
            break;

        case TOKEN_STAR:
            printf("%s", "TOKEN_STAR");
            break;

        case TOKEN_STARSTAR:
            printf("%s", "TOKEN_STARSTAR");
            break;

        case TOKEN_SLASH:
            printf("%s", "TOKEN_SLASH");
            break;

        case TOKEN_PERCENT:
            printf("%s", "TOKEN_PERCENT");
            break;

        case TOKEN_EQUAL:
            printf("%s", "TOKEN_EQUAL");
            break;

        case TOKEN_EQEQUAL:
            printf("%s", "TOKEN_EQEQUAL");
            break;

        case TOKEN_GREATER:
            printf("%s", "TOKEN_GREATER");
            break;

        case TOKEN_GREATEREQ:
            printf("%s", "TOKEN_GREATEREQ");
            break;

        case TOKEN_LESS:
            printf("%s", "TOKEN_LESS");
            break;

        case TOKEN_LESSEQ:
            printf("%s", "TOKEN_LESSEQ");
            break;

        case TOKEN_NOTEQ:
            printf("%s", "TOKEN_NOTEQ");
            break;

        case TOKEN_FOR:
            printf("%s", "TOKEN_FOR");
            break;

        case TOKEN_WHILE:
            printf("%s", "TOKEN_WHILE");
            break;

        case TOKEN_BREAK:
            printf("%s", "TOKEN_BREAK");
            break;

        case TOKEN_SKIP:
            printf("%s", "TOKEN_SKIP");
            break;

        case TOKEN_IN:
            printf("%s", "TOKEN_IN");
            break;

        case TOKEN_WITH:
            printf("%s", "TOKEN_WITH");
            break;

        case TOKEN_IF:
            printf("%s", "TOKEN_IF");
            break;

        case TOKEN_ELSE:
            printf("%s", "TOKEN_ELSE");
            break;

        case TOKEN_CLASS:
            printf("%s", "TOKEN_CLASS");
            break;

        case TOKEN_SELF:
            printf("%s", "TOKEN_SELF");
            break;

        case TOKEN_SUPER:
            printf("%s", "TOKEN_SUPER");
            break;

        case TOKEN_LET:
            printf("%s", "TOKEN_LET");
            break;

        case TOKEN_FN:
            printf("%s", "TOKEN_FN");
            break;

        case TOKEN_RETURN:
            printf("%s", "TOKEN_RETURN");
            break;

        case TOKEN_IMPORT:
            printf("%s", "TOKEN_IMPORT");
            break;

        case TOKEN_AND:
            printf("%s", "TOKEN_AND");
            break;

        case TOKEN_NOT:
            printf("%s", "TOKEN_NOT");
            break;

        case TOKEN_OR:
            printf("%s", "TOKEN_OR");
            break;

        case TOKEN_TRUE:
            printf("%s", "TOKEN_TRUE");
            break;

        case TOKEN_FALSE:
            printf("%s", "TOKEN_FALSE");
            break;

        case TOKEN_NONE:
            printf("%s", "TOKEN_NONE");
            break;

        case TOKEN_NAME:
            printf("%s", "TOKEN_NAME");
            break;

        case TOKEN_NUMBER:
            printf("%s", "TOKEN_NUMBER");
            break;

        case TOKEN_STRING:
            printf("%s", "TOKEN_STRING");
            break;

        case TOKEN_LINE:
            printf("%s", "TOKEN_LINE");
            break;

        case TOKEN_ERROR:
            printf("%s", "TOKEN_ERROR");
            break;

        case TOKEN_EOF:
            printf("%s", "TOKEN_EOF");
            break;

        default:
            printf("%s", "unknown token type");
            break;
    }
}
