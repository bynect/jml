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
    TOKEN_PLUSEQ,
    TOKEN_MINUS,
    TOKEN_MINUSEQ,
    TOKEN_STAR,
    TOKEN_STAREQ,
    TOKEN_STARSTAR,
    TOKEN_STARSTAREQ,
    TOKEN_SLASH,
    TOKEN_SLASHEQ,
    TOKEN_PERCENT,
    TOKEN_PERCENTEQ,
    TOKEN_EQUAL,

    /* logic operators */
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
    TOKEN_ASYNC,
    TOKEN_AWAIT,
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
    uint32_t                        length;
    uint32_t                        line;
    uint32_t                        offset;
} jml_token_t;


typedef struct {
    const char                     *source;
    const char                     *start;
    const char                     *current;
    uint32_t                        line;
    bool                            eof;
    bool                            comment;
} jml_lexer_t;


void jml_lexer_init(jml_lexer_t *lexer, const char *source);

jml_token_t jml_lexer_tokenize(jml_lexer_t *lexer);

void jml_token_type_print(jml_token_type type);


#endif /* JML_LEXER_H_ */
