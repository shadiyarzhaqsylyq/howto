#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
//Parsing query
/* ========================================================================= *
 * 1. LEXER (TOKENIZER)
 * ========================================================================= */

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_CREATE,
    TOKEN_TABLE,
    TOKEN_INT,
    TOKEN_VARCHAR,
    TOKEN_PRIMARY,
    TOKEN_KEY,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
} Token;

typedef struct {
    const char *source;
    int cursor;
} Lexer;

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->cursor = 0;
}

static char lexer_peek(Lexer *lexer) {
    return lexer->source[lexer->cursor];
}

static char lexer_advance(Lexer *lexer) {
    if (lexer->source[lexer->cursor] == '\0') return '\0';
    return lexer->source[lexer->cursor++];
}

static void lexer_skip_whitespace(Lexer *lexer) {
    while (isspace((unsigned char)lexer_peek(lexer))) {
        lexer_advance(lexer);
    }
}

static bool str_equals_case_insensitive(const char *a, const char *b, int len) {
    for (int i = 0; i < len; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
            return false;
        }
    }
    return b[len] == '\0';
}

static TokenType check_keyword_or_ident(const char *start, int len) {
    if (str_equals_case_insensitive(start, "CREATE", len))  return TOKEN_CREATE;
    if (str_equals_case_insensitive(start, "TABLE", len))   return TOKEN_TABLE;
    if (str_equals_case_insensitive(start, "INT", len))     return TOKEN_INT;
    if (str_equals_case_insensitive(start, "VARCHAR", len)) return TOKEN_VARCHAR;
    if (str_equals_case_insensitive(start, "PRIMARY", len)) return TOKEN_PRIMARY;
    if (str_equals_case_insensitive(start, "KEY", len))     return TOKEN_KEY;
    return TOKEN_IDENTIFIER;
}

Token lexer_next_token(Lexer *lexer) {
    lexer_skip_whitespace(lexer);
    const char *start = &lexer->source[lexer->cursor];
    char c = lexer_advance(lexer);

    if (c == '\0') {
        return (Token){TOKEN_EOF, start, 0};
    }

    switch (c) {
        case '(': return (Token){TOKEN_LPAREN, start, 1};
        case ')': return (Token){TOKEN_RPAREN, start, 1};
        case ',': return (Token){TOKEN_COMMA, start, 1};
        case ';': return (Token){TOKEN_SEMICOLON, start, 1};
    }

    if (isdigit((unsigned char)c)) {
        while (isdigit((unsigned char)lexer_peek(lexer))) {
            lexer_advance(lexer);
        }
        return (Token){TOKEN_NUMBER, start, (int)(&lexer->source[lexer->cursor] - start)};
    }

    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)lexer_peek(lexer)) || lexer_peek(lexer) == '_') {
            lexer_advance(lexer);
        }
        int len = (int)(&lexer->source[lexer->cursor] - start);
        TokenType type = check_keyword_or_ident(start, len);
        return (Token){type, start, len};
    }

    return (Token){TOKEN_ERROR, start, 1};
}

/* ========================================================================= *
 * 2. TEMPORARY LINKED-LIST AST (only used during parsing)
 * ========================================================================= */

typedef enum {
    DATA_TYPE_INT,
    DATA_TYPE_VARCHAR
} DataType;

typedef struct ColumnDef {
    char *name;
    DataType type;
    int varchar_length;
    bool is_primary_key;
    struct ColumnDef *next;
} ColumnDef;

typedef struct {
    char *table_name;
    ColumnDef *columns;
    int column_count;
} CreateTableStmt;

/* ========================================================================= *
 * 3. FLAT REPRESENTATION (the one we keep)
 * ========================================================================= */

typedef struct {
    char *name;
    DataType type;
    int varchar_length;
    bool is_primary_key;
} FlatColumn;

typedef struct {
    char *table_name;
    FlatColumn *columns;      /* contiguous array */
    int column_count;
} FlatCreateTable;

/* ========================================================================= *
 * 4. HELPERS
 * ========================================================================= */

static char *token_to_string(Token token) {
    char *str = (char *)malloc(token.length + 1);
    if (!str) return NULL;
    memcpy(str, token.start, token.length);
    str[token.length] = '\0';
    return str;
}

/* ========================================================================= *
 * 5. PARSER
 * ========================================================================= */

typedef struct {
    Lexer *lexer;
    Token current;
    Token peek;
} Parser;

static void parser_advance(Parser *parser) {
    parser->current = parser->peek;
    parser->peek = lexer_next_token(parser->lexer);
}

void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->current = lexer_next_token(lexer);
    parser->peek = lexer_next_token(lexer);
}

static bool parser_match(Parser *parser, TokenType type) {
    if (parser->current.type == type) {
        parser_advance(parser);
        return true;
    }
    return false;
}

static bool parser_expect(Parser *parser, TokenType type, const char *err_msg) {
    if (parser->current.type == type) {
        parser_advance(parser);
        return true;
    }
    fprintf(stderr, "Syntax Error: %s. Got '%.*s'\n",
            err_msg, parser->current.length, parser->current.start);
    return false;
}

static char *parse_identifier(Parser *parser) {
    if (parser->current.type == TOKEN_IDENTIFIER ||
        parser->current.type == TOKEN_TABLE ||
        parser->current.type == TOKEN_KEY) {

        char *id = token_to_string(parser->current);
        parser_advance(parser);
        return id;
    }
    fprintf(stderr, "Syntax Error: Expected identifier, got '%.*s'\n",
            parser->current.length, parser->current.start);
    return NULL;
}

static ColumnDef *parse_column_def(Parser *parser) {
    char *col_name = parse_identifier(parser);
    if (!col_name) return NULL;

    DataType type;
    int varchar_len = 255;

    if (parser_match(parser, TOKEN_INT)) {
        type = DATA_TYPE_INT;
    } else if (parser_match(parser, TOKEN_VARCHAR)) {
        type = DATA_TYPE_VARCHAR;
        if (parser_match(parser, TOKEN_LPAREN)) {
            if (parser->current.type == TOKEN_NUMBER) {
                varchar_len = atoi(parser->current.start);
                parser_advance(parser);
            } else {
                fprintf(stderr, "Syntax Error: Expected number inside VARCHAR(...)\n");
                free(col_name);
                return NULL;
            }
            if (!parser_expect(parser, TOKEN_RPAREN, "Expected ')' after VARCHAR length")) {
                free(col_name);
                return NULL;
            }
        }
    } else {
        fprintf(stderr, "Syntax Error: Expected data type for column '%s'\n", col_name);
        free(col_name);
        return NULL;
    }

    bool is_primary_key = false;
    if (parser_match(parser, TOKEN_PRIMARY)) {
        if (!parser_expect(parser, TOKEN_KEY, "Expected 'KEY' after 'PRIMARY'")) {
            free(col_name);
            return NULL;
        }
        is_primary_key = true;
    }

    ColumnDef *col = (ColumnDef *)malloc(sizeof(ColumnDef));
    if (!col) {
        free(col_name);
        return NULL;
    }
    col->name = col_name;
    col->type = type;
    col->varchar_length = varchar_len;
    col->is_primary_key = is_primary_key;
    col->next = NULL;
    return col;
}

CreateTableStmt *parse_create_table(Parser *parser) {
    if (!parser_expect(parser, TOKEN_CREATE, "Expected 'CREATE'")) return NULL;
    if (!parser_expect(parser, TOKEN_TABLE, "Expected 'TABLE'")) return NULL;

    char *table_name = parse_identifier(parser);
    if (!table_name) return NULL;

    if (!parser_expect(parser, TOKEN_LPAREN, "Expected '(' after table name")) {
        free(table_name);
        return NULL;
    }

    CreateTableStmt *stmt = (CreateTableStmt *)malloc(sizeof(CreateTableStmt));
    if (!stmt) {
        free(table_name);
        return NULL;
    }
    stmt->table_name = table_name;
    stmt->columns = NULL;
    stmt->column_count = 0;

    ColumnDef **tail = &stmt->columns;

    while (parser->current.type != TOKEN_RPAREN && parser->current.type != TOKEN_EOF) {
        ColumnDef *col = parse_column_def(parser);
        if (!col) {
            /* Simple cleanup on error */
            free(stmt->table_name);
            ColumnDef *c = stmt->columns;
            while (c) {
                ColumnDef *n = c->next;
                free(c->name);
                free(c);
                c = n;
            }
            free(stmt);
            return NULL;
        }

        *tail = col;
        tail = &col->next;
        stmt->column_count++;

        if (!parser_match(parser, TOKEN_COMMA)) {
            break;
        }
    }

    if (!parser_expect(parser, TOKEN_RPAREN, "Expected ')' after column list")) {
        free(stmt->table_name);
        ColumnDef *c = stmt->columns;
        while (c) {
            ColumnDef *n = c->next;
            free(c->name);
            free(c);
            c = n;
        }
        free(stmt);
        return NULL;
    }

    parser_match(parser, TOKEN_SEMICOLON);  /* optional */
    return stmt;
}

/* ========================================================================= *
 * 6. LOWERING: linked-list AST → flat array
 * ========================================================================= */

FlatCreateTable *lower_to_flat(CreateTableStmt *stmt) {
    if (!stmt) return NULL;

    FlatCreateTable *flat = (FlatCreateTable *)malloc(sizeof(FlatCreateTable));
    if (!flat) return NULL;

    flat->table_name = stmt->table_name;          /* take ownership */
    flat->column_count = stmt->column_count;
    flat->columns = (FlatColumn *)malloc(sizeof(FlatColumn) * stmt->column_count);

    if (!flat->columns && stmt->column_count > 0) {
        free(flat);
        return NULL;
    }

    ColumnDef *curr = stmt->columns;
    for (int i = 0; i < stmt->column_count; i++) {
        flat->columns[i].name           = curr->name;           /* take ownership */
        flat->columns[i].type           = curr->type;
        flat->columns[i].varchar_length = curr->varchar_length;
        flat->columns[i].is_primary_key = curr->is_primary_key;

        ColumnDef *next = curr->next;
        free(curr);          /* free only the node shell */
        curr = next;
    }

    free(stmt);              /* free only the outer struct */
    return flat;
}

/* ========================================================================= *
 * 7. CLEANUP & PRINTING (flat version)
 * ========================================================================= */

void free_flat_create_table(FlatCreateTable *flat) {
    if (!flat) return;
    free(flat->table_name);
    for (int i = 0; i < flat->column_count; i++) {
        free(flat->columns[i].name);
    }
    free(flat->columns);
    free(flat);
}

void print_flat(const FlatCreateTable *flat) {
    if (!flat) return;

    printf("FlatCreateTable (contiguous array):\n");
    printf("  Table Name : %s\n", flat->table_name);
    printf("  Columns    : %d\n", flat->column_count);
    printf("  ---------------------------------------------\n");

    for (int i = 0; i < flat->column_count; i++) {
        const FlatColumn *c = &flat->columns[i];
        printf("  [%d] %-10s | %-7s", i, c->name,
               c->type == DATA_TYPE_INT ? "INT" : "VARCHAR");

        if (c->type == DATA_TYPE_VARCHAR) {
            printf("(%d)", c->varchar_length);
        } else {
            printf("     ");
        }

        printf(" | PK: %s\n", c->is_primary_key ? "YES" : "NO");
    }
}

/* ========================================================================= *
 * 8. MAIN
 * ========================================================================= */

int main(void) {
    const char *query =
        "CREATE TABLE table (id INT PRIMARY KEY, name VARCHAR, did VARCHAR, "
        "dep VARCHAR, salary INT, city VARCHAR);";

    printf("Input Query:\n%s\n\n", query);

    Lexer lexer;
    lexer_init(&lexer, query);

    Parser parser;
    parser_init(&parser, &lexer);

    /* 1. Parse into temporary linked-list AST */
    CreateTableStmt *ast = parse_create_table(&parser);

    if (!ast) {
        printf("Failed to parse query.\n");
        return 1;
    }

    /* 2. Immediately lower to flat representation and discard the tree */
    FlatCreateTable *flat = lower_to_flat(ast);

    if (!flat) {
        printf("Failed to lower AST.\n");
        return 1;
    }

    /* 3. From this point we only work with the flat version */
    print_flat(flat);

    free_flat_create_table(flat);
    return 0;
}
