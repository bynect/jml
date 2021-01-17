#ifndef JML_LEXER_H_
#define JML_LEXER_H_

#include <jml_common.h>


typedef enum {
    /* bracket delimiters */
    TOKEN_RPAREN,
    TOKEN_LPAREN,
    TOKEN_RSQARE,
    TOKEN_LSQARE,
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
    TOKEN_FN,
    TOKEN_RETURN,
    TOKEN_IMPORT,
    TOKEN_AND,
    TOKEN_NOT,
    TOKEN_OR,

    /* literals */
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NONE,
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
    int                             length;
    int                             line;
} jml_token_t;


typedef struct {
    const char                     *start;
    const char                     *current;
    int                             line;
    bool                            eof;
} jml_lexer_t;


void jml_lexer_init(const char *source);

jml_token_t jml_lexer_tokenize(void);

void jml_token_type_print(jml_token_type type);


#endif /* JML_LEXER_H_ */
