#ifndef _JML_LEXER_H_
#define _JML_LEXER_H_

#include <jml_common.h>


typedef enum {
    /* bracket delimiters */
    TOKEN_RPAREN,
    TOKEN_LPAREN,
    TOKEN_LSQARE,
    TOKEN_RSQARE,
    TOKEN_RBRACE,
    TOKEN_LBRACE,

    /* dotted delimiters */
    TOKEN_COLON,
    TOKEN_SEMI,
    TOKEN_COMMA,
    TOKEN_DOT,

    /* special characters */
    TOKEN_PIPE,
    TOKEN_CARET,
    TOKEN_AMP,
    TOKEN_TILDE,
    TOKEN_QUEST,
    TOKEN_BANG,
    TOKEN_HASH,
    TOKEN_AT,
    TOKEN_ARROW,

    /* arithmetic operators */
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_STARSTAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,

    /* logic operators */
    TOKEN_EQUAL,
    TOKEN_EQEQUAL,
    TOKEN_GREATER,
    TOKEN_GREATEREQ,
    TOKEN_LESS,
    TOKEN_LESSEQ,
    TOKEN_NOTEQ,

    /* keywords */
    TOKEN_FOR,
    TOKEN_WHILE,
    TOKEN_BREAK,
    TOKEN_SKIP,
    TOKEN_IN,
    TOKEN_WITH,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_CLASS,
    TOKEN_SELF,
    TOKEN_SUPER,
    TOKEN_LET,
    TOKEN_FUNC,
    TOKEN_RETURN,
    TOKEN_IMPORT,
    TOKEN_AND,
    TOKEN_NOT,
    TOKEN_OR,

    /* atomics */
    TOKEN_ATOM,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NONE,

    /* literals */
    TOKEN_NAME,
    TOKEN_NUMBER,
    TOKEN_STRING,

    /* reserved tokens */
    TOKEN_LINE,
    TOKEN_ERROR,
    TOKEN_EOF
} jml_token_type;


typedef struct {
    jml_token_type                  type;
    const char                     *start;
    size_t                          length;
    uint16_t                        line;
} jml_token_t;


typedef struct {
    const char                     *start;
    const char                     *current;
    uint16_t                        line;
} jml_lexer_t;


void jml_lexer_init(const char *source);


jml_token_t jml_lexer_tokenize(void);


static inline bool
jml_is_digit(char c)
{
    return c >= '0' && c <= '9';
}


static inline bool
jml_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c == '_');
}


static inline bool
jml_is_bracket(char c)
{
    return (c == '(' || c == ')')
        || (c == '[' || c == ']')
        || (c == '{' || c == '}');
}


#endif /* _JML_LEXER_H_ */
