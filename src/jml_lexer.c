#include <string.h>
#include <stdio.h>

#include <jml/jml_lexer.h>
#include <jml/jml_util.h>


void
jml_lexer_init(jml_lexer_t *lexer, const char *source)
{
    lexer->source   = source;
    lexer->start    = source;
    lexer->current  = source;
    lexer->line     = 1;
    lexer->eof      = false;
    lexer->comment  = false;
}


static inline bool
jml_lexer_eof(jml_lexer_t *lexer)
{
    return *lexer->current == '\0';
}


static inline char
jml_lexer_peek(jml_lexer_t *lexer)
{
    return *lexer->current;
}


static inline char
jml_lexer_peek_next(jml_lexer_t *lexer)
{
    if (jml_lexer_eof(lexer))
        return '\0';

    return lexer->current[1];
}


static inline char
jml_lexer_peek_previous(jml_lexer_t *lexer)
{
    return lexer->current[-1];
}


static inline char
jml_lexer_advance(jml_lexer_t *lexer)
{
    ++lexer->current;
    return lexer->current[-1];
}


static inline void
jml_lexer_newline(jml_lexer_t *lexer)
{
    ++lexer->line;
}


static bool
jml_lexer_match(char expected, jml_lexer_t *lexer)
{
    if (jml_lexer_eof(lexer))           return false;
    if (*lexer->current != expected)    return false;

    ++lexer->current;
    return true;
}


static jml_token_t
jml_token_emit(jml_token_type type, jml_lexer_t *lexer)
{
    jml_token_t token;
    token.type      = type;
    token.start     = lexer->start;
    token.length    = lexer->current - lexer->start;
    token.line      = lexer->line;
    token.offset    = lexer->start - lexer->source;

    return token;
}


static jml_token_t
jml_token_emit_error(const char *message, jml_lexer_t *lexer)
{
    jml_token_t token;
    token.type      = TOKEN_ERROR;
    token.start     = message;
    token.length    = strlen(message);
    token.line      = lexer->line;
    token.offset    = lexer->start - lexer->source;

    return token;
}


static void
jml_lexer_skip_char(jml_lexer_t *lexer)
{
    while (true) {
        char c = jml_lexer_peek(lexer);

        if (lexer->comment && c != '?' && c != '\n') {
            if (jml_lexer_eof(lexer))
                return;

            jml_lexer_advance(lexer);
            continue;
        }

        switch (c) {
            case  ' ':
            case '\t':
            case '\r':
                jml_lexer_advance(lexer);
                break;

            case '\n':
                jml_lexer_newline(lexer);
                if (lexer->comment) {
                    jml_lexer_advance(lexer);
                    break;
                } else
                    return;

            case  '!':
                if (jml_lexer_peek_next(lexer) == '=') return;
                while (jml_lexer_peek(lexer) != '\n' && !jml_lexer_eof(lexer))
                    jml_lexer_advance(lexer);

                if (jml_lexer_peek(lexer) == '\n') {
                    jml_lexer_newline(lexer);
                    jml_lexer_advance(lexer);
                }
                break;

            case '?':
                lexer->comment = !lexer->comment;
                jml_lexer_advance(lexer);
                break;

            default:
                return;
        }
    }
}


static jml_token_type
jml_keyword_match(int start, int length, const char *rest,
    jml_token_type type, jml_lexer_t *lexer)
{
    if ((lexer->current - lexer->start) == (length + start)
        && memcmp(lexer->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_NAME;
}


static jml_token_type
jml_identifier_check(jml_lexer_t *lexer)
{
    switch (lexer->start[0]) {
        case 'a':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 's': return jml_keyword_match(2, 3, "ync", TOKEN_ASYNC, lexer);
                    case 'n': return jml_keyword_match(2, 1, "d", TOKEN_AND, lexer);
                    case 'w': return jml_keyword_match(2, 3, "ait", TOKEN_AWAIT, lexer);
                }
            }
            break;

        case 'b': return jml_keyword_match(1, 4, "reak", TOKEN_BREAK, lexer);
        case 'c': return jml_keyword_match(1, 4, "lass", TOKEN_CLASS, lexer);
        case 'e': return jml_keyword_match(1, 3, "lse", TOKEN_ELSE, lexer);

        case 'f':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'a': return jml_keyword_match(2, 3, "lse", TOKEN_FALSE, lexer);
                    case 'n': return jml_keyword_match(2, 0, "", TOKEN_FN, lexer);
                    case 'o': return jml_keyword_match(2, 1, "r", TOKEN_FOR, lexer);
                    case 'r': return jml_keyword_match(2, 2, "om", TOKEN_FROM, lexer);
                }
            }
            break;

        case 'i':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'f': return jml_keyword_match(2, 0, "", TOKEN_IF, lexer);
                    case 'n': return jml_keyword_match(2, 0, "", TOKEN_IN, lexer);
                    case 'm': return jml_keyword_match(2, 4, "port", TOKEN_IMPORT, lexer);
                }
            }
            break;

        case 'n':
            if (lexer->current - lexer->start > 1
                && lexer->start[1] == 'o') {
                switch (lexer->start[2]) {
                    case 'n': return jml_keyword_match(3, 1, "e", TOKEN_NONE, lexer);
                    case 't': return jml_keyword_match(3, 0, "", TOKEN_NOT, lexer);
                }
            }
            break;

        case 'm': return jml_keyword_match(1, 4, "atch", TOKEN_MATCH, lexer);
        case 'o': return jml_keyword_match(1, 1, "r", TOKEN_OR, lexer);
        case 'l': return jml_keyword_match(1, 2, "et", TOKEN_LET, lexer);
        case 'r': return jml_keyword_match(1, 5, "eturn", TOKEN_RETURN, lexer);

        case 's':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'e': return jml_keyword_match(2, 2, "lf", TOKEN_SELF, lexer);
                    case 'k': return jml_keyword_match(2, 2, "ip", TOKEN_SKIP, lexer);
                    case 'u': return jml_keyword_match(2, 3, "per", TOKEN_SUPER, lexer);
                    case 'p': return jml_keyword_match(2, 4, "read", TOKEN_SPREAD, lexer);
                }
            }
            break;

        case 't':
            if (lexer->current - lexer->start > 2 && lexer->start[1] == 'r') {
                switch (lexer->start[2]) {
                    case 'u': return jml_keyword_match(3, 1, "e", TOKEN_TRUE, lexer);
                    case 'y': return jml_keyword_match(3, 0, "", TOKEN_TRY, lexer);
                }
            }
            break;

        case 'w':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'i': return jml_keyword_match(2, 2, "th", TOKEN_WITH, lexer);
                    case 'h': return jml_keyword_match(2, 3, "ile", TOKEN_WHILE, lexer);
                }
            }
            break;

        case '_': return jml_keyword_match(1, 0, "", TOKEN_USCORE, lexer);
    }
    return TOKEN_NAME;
}


static jml_token_t
jml_identifier_literal(jml_lexer_t *lexer)
{
    while (jml_is_ident(jml_lexer_peek(lexer))
        || jml_is_digit(jml_lexer_peek(lexer)))
        jml_lexer_advance(lexer);

    return jml_token_emit(jml_identifier_check(lexer), lexer);
}


static jml_token_t
jml_string_literal(const char delimiter, jml_lexer_t *lexer)
{
    while (true) {
        if (jml_lexer_eof(lexer))
            return jml_token_emit_error("Unterminated string.", lexer);

        char c = jml_lexer_peek(lexer);

        if (c == '\n')
            jml_lexer_newline(lexer);
        else if (c == delimiter)
            break;
        else if (c == '\\' && jml_lexer_peek_next(lexer) == delimiter)
            jml_lexer_advance(lexer);

        jml_lexer_advance(lexer);
    }

    jml_lexer_advance(lexer);
    return jml_token_emit(TOKEN_STRING, lexer);
}


static jml_token_t
jml_number_literal(jml_lexer_t *lexer)
{
    if (jml_lexer_peek_previous(lexer) == '0') {
        switch (jml_lexer_peek(lexer)) {
            case 'x':
            case 'X': {
                jml_lexer_advance(lexer);
                if (!jml_is_hex(jml_lexer_peek(lexer)))
                    return jml_token_emit_error("Unterminated hexadecimal literal.", lexer);

                while (jml_is_hex(jml_lexer_peek(lexer)))
                    jml_lexer_advance(lexer);

                if ((jml_lexer_peek(lexer) == '.'))
                    return jml_token_emit_error("Invalid dot after hexadecimal literal.", lexer);

                return jml_token_emit(TOKEN_NUMBER, lexer);
            }

            case 'o':
            case 'O': {
                jml_lexer_advance(lexer);
                if (!jml_is_oct(jml_lexer_peek(lexer)))
                    return jml_token_emit_error("Unterminated octal literal.", lexer);

                while (jml_is_oct(jml_lexer_peek(lexer)))
                    jml_lexer_advance(lexer);

                if ((jml_lexer_peek(lexer) == '.'))
                    return jml_token_emit_error("Invalid dot after octal literal.", lexer);

                return jml_token_emit(TOKEN_NUMBER, lexer);
            }

            case 'b':
            case 'B': {
                jml_lexer_advance(lexer);
                if (!jml_is_bin(jml_lexer_peek(lexer)))
                    return jml_token_emit_error("Unterminated binary literal.", lexer);

                while (jml_is_bin(jml_lexer_peek(lexer)))
                    jml_lexer_advance(lexer);

                if ((jml_lexer_peek(lexer) == '.'))
                    return jml_token_emit_error("Invalid dot after binary literal.", lexer);

                return jml_token_emit(TOKEN_NUMBER, lexer);
            }

            case '0':
                return jml_token_emit_error("Invalid leading zeroes.", lexer);

            default:
                break;
        }
    }

    while (jml_is_digit(jml_lexer_peek(lexer)))
        jml_lexer_advance(lexer);

    if ((jml_lexer_peek(lexer) == '.')
        && jml_is_digit(jml_lexer_peek_next(lexer))) {

        jml_lexer_advance(lexer);
        while (jml_is_digit(jml_lexer_peek(lexer)))
            jml_lexer_advance(lexer);
    }

    return jml_token_emit(TOKEN_NUMBER, lexer);
}


jml_token_t
jml_lexer_tokenize(jml_lexer_t *lexer)
{
    if (lexer->eof)         return jml_token_emit(TOKEN_EOF, lexer);

    jml_lexer_skip_char(lexer);
    lexer->start    =       lexer->current;

    if (jml_lexer_eof(lexer)) {
        lexer->eof  =       true;

        return  lexer->comment
            ? jml_token_emit_error("Unterminated comment.", lexer)
            : jml_token_emit(TOKEN_LINE, lexer);
    }

    char c          =       jml_lexer_advance(lexer);
    if (jml_is_ident(c))    return jml_identifier_literal(lexer);
    if (jml_is_digit(c))    return jml_number_literal(lexer);

    switch (c) {
        case  ')':  return  jml_token_emit(TOKEN_RPAREN, lexer);
        case  '(':  return  jml_token_emit(TOKEN_LPAREN, lexer);
        case  ']':  return  jml_token_emit(TOKEN_RSQARE, lexer);
        case  '[':  return  jml_token_emit(TOKEN_LSQARE, lexer);
        case  '}':  return  jml_token_emit(TOKEN_RBRACE, lexer);
        case  '{':  return  jml_token_emit(TOKEN_LBRACE, lexer);
        case  ';':  return  jml_token_emit(TOKEN_SEMI, lexer);
        case  ',':  return  jml_token_emit(TOKEN_COMMA, lexer);
        case  '.':  return  jml_token_emit(TOKEN_DOT, lexer);

        case  '_':  return  jml_token_emit(TOKEN_USCORE, lexer);
        case  '^':  return  jml_token_emit(TOKEN_CARET, lexer);
        case  '&':  return  jml_token_emit(TOKEN_AMP, lexer);
        case  '~':  return  jml_token_emit(TOKEN_TILDE, lexer);
        case  '?':  return  jml_token_emit(TOKEN_QUEST, lexer);
        case  '#':  return  jml_token_emit(TOKEN_HASH, lexer);
        case  '@':  return  jml_token_emit(TOKEN_AT, lexer);

        case  '|':  return  jml_token_emit(jml_lexer_match('>', lexer)
                        ? TOKEN_PIPE : TOKEN_VBAR, lexer);

        case  ':':  return  jml_token_emit(jml_lexer_match(':', lexer)
                        ? jml_lexer_match('=', lexer) ? TOKEN_COLCOLONEQ
                        : TOKEN_COLCOLON : TOKEN_COLON, lexer);

        case  '+':  return  jml_token_emit(jml_lexer_match('=', lexer)
                        ? TOKEN_PLUSEQ : TOKEN_PLUS, lexer);

        case  '-':  return  jml_token_emit(jml_lexer_match('>', lexer)
                        ? TOKEN_RARROW : jml_lexer_match('=', lexer)
                        ? TOKEN_MINUSEQ : TOKEN_MINUS, lexer);

        case  '*':  return  jml_token_emit(jml_lexer_match('*', lexer)
                        ? jml_lexer_match('=', lexer) ? TOKEN_STARSTAREQ
                        : TOKEN_STARSTAR : jml_lexer_match('=', lexer)
                        ? TOKEN_STAREQ : TOKEN_STAR, lexer);

        case  '/':  return  jml_token_emit(jml_lexer_match('=', lexer)
                        ? TOKEN_SLASHEQ : TOKEN_SLASH, lexer);

        case  '%':  return  jml_token_emit(jml_lexer_match('=', lexer)
                        ? TOKEN_PERCENTEQ : TOKEN_PERCENT, lexer);

        case  '=':  return  jml_token_emit(jml_lexer_match('=', lexer)
                        ? TOKEN_EQEQUAL : TOKEN_EQUAL, lexer);

        case  '<':  return  jml_token_emit(jml_lexer_match('-', lexer)
                        ? TOKEN_LARROW : jml_lexer_match('=', lexer)
                        ? TOKEN_LESSEQ : TOKEN_LESS, lexer);

        case  '>':  return  jml_token_emit(jml_lexer_match('=', lexer)
                        ? TOKEN_GREATEREQ : TOKEN_GREATER, lexer);

        case  '!':  return  jml_token_emit(jml_lexer_match('=', lexer)
                        ? TOKEN_NOTEQ : TOKEN_BANG, lexer);

        case '\'':  return  jml_string_literal('\'', lexer);
        case  '"':  return  jml_string_literal('"', lexer);

        case '\n':  return  jml_token_emit(TOKEN_LINE, lexer);

        default:
            break;
    }

    return jml_token_emit_error("Unexpected character.", lexer);
}


void
jml_token_type_print(jml_token_type type)
{
#define PRINT_TOKEN(type)           type: printf("%s", #type); break


    switch(type) {
        case PRINT_TOKEN(TOKEN_RPAREN);
        case PRINT_TOKEN(TOKEN_LPAREN);
        case PRINT_TOKEN(TOKEN_RSQARE);
        case PRINT_TOKEN(TOKEN_LSQARE);
        case PRINT_TOKEN(TOKEN_RBRACE);
        case PRINT_TOKEN(TOKEN_LBRACE);
        case PRINT_TOKEN(TOKEN_SEMI);
        case PRINT_TOKEN(TOKEN_COMMA);
        case PRINT_TOKEN(TOKEN_DOT);

        case PRINT_TOKEN(TOKEN_USCORE);
        case PRINT_TOKEN(TOKEN_CARET);
        case PRINT_TOKEN(TOKEN_AMP);
        case PRINT_TOKEN(TOKEN_TILDE);
        case PRINT_TOKEN(TOKEN_QUEST);
        case PRINT_TOKEN(TOKEN_BANG);
        case PRINT_TOKEN(TOKEN_HASH);
        case PRINT_TOKEN(TOKEN_AT);
        case PRINT_TOKEN(TOKEN_RARROW);
        case PRINT_TOKEN(TOKEN_LARROW);
        case PRINT_TOKEN(TOKEN_VBAR);
        case PRINT_TOKEN(TOKEN_PIPE);
        case PRINT_TOKEN(TOKEN_COLON);

        case PRINT_TOKEN(TOKEN_COLCOLON);
        case PRINT_TOKEN(TOKEN_COLCOLONEQ);
        case PRINT_TOKEN(TOKEN_PLUS);
        case PRINT_TOKEN(TOKEN_PLUSEQ);
        case PRINT_TOKEN(TOKEN_MINUS);
        case PRINT_TOKEN(TOKEN_MINUSEQ);
        case PRINT_TOKEN(TOKEN_STAR);
        case PRINT_TOKEN(TOKEN_STAREQ);
        case PRINT_TOKEN(TOKEN_STARSTAR);
        case PRINT_TOKEN(TOKEN_STARSTAREQ);
        case PRINT_TOKEN(TOKEN_SLASH);
        case PRINT_TOKEN(TOKEN_SLASHEQ);
        case PRINT_TOKEN(TOKEN_PERCENT);
        case PRINT_TOKEN(TOKEN_PERCENTEQ);
        case PRINT_TOKEN(TOKEN_EQUAL);

        case PRINT_TOKEN(TOKEN_EQEQUAL);
        case PRINT_TOKEN(TOKEN_GREATER);
        case PRINT_TOKEN(TOKEN_GREATEREQ);
        case PRINT_TOKEN(TOKEN_LESS);
        case PRINT_TOKEN(TOKEN_LESSEQ);
        case PRINT_TOKEN(TOKEN_NOTEQ);

        case PRINT_TOKEN(TOKEN_FOR);
        case PRINT_TOKEN(TOKEN_WHILE);
        case PRINT_TOKEN(TOKEN_BREAK);
        case PRINT_TOKEN(TOKEN_SKIP);
        case PRINT_TOKEN(TOKEN_IN);
        case PRINT_TOKEN(TOKEN_MATCH);
        case PRINT_TOKEN(TOKEN_WITH);
        case PRINT_TOKEN(TOKEN_IF);
        case PRINT_TOKEN(TOKEN_ELSE);
        case PRINT_TOKEN(TOKEN_CLASS);
        case PRINT_TOKEN(TOKEN_SELF);
        case PRINT_TOKEN(TOKEN_SUPER);
        case PRINT_TOKEN(TOKEN_LET);
        case PRINT_TOKEN(TOKEN_FN);
        case PRINT_TOKEN(TOKEN_RETURN);
        case PRINT_TOKEN(TOKEN_IMPORT);
        case PRINT_TOKEN(TOKEN_FROM);
        case PRINT_TOKEN(TOKEN_ASYNC);
        case PRINT_TOKEN(TOKEN_AWAIT);
        case PRINT_TOKEN(TOKEN_TRY);
        case PRINT_TOKEN(TOKEN_SPREAD);
        case PRINT_TOKEN(TOKEN_AND);
        case PRINT_TOKEN(TOKEN_NOT);
        case PRINT_TOKEN(TOKEN_OR);

        case PRINT_TOKEN(TOKEN_TRUE);
        case PRINT_TOKEN(TOKEN_FALSE);
        case PRINT_TOKEN(TOKEN_NONE);
        case PRINT_TOKEN(TOKEN_NAME);
        case PRINT_TOKEN(TOKEN_NUMBER);
        case PRINT_TOKEN(TOKEN_STRING);

        case PRINT_TOKEN(TOKEN_LINE);
        case PRINT_TOKEN(TOKEN_ERROR);
        case PRINT_TOKEN(TOKEN_EOF);

        default:
            printf("%s", "unknown token type");
            break;
    }


#undef PRINT_TOKEN
}
