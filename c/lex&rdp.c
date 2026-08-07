#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
// Lexer and Recursive Descent Parser


// ==========================================
// 1. LEXER IMPLEMENTATION
// ==========================================

typedef enum {
    TOKEN_INT, TOKEN_IDENTIFIER, TOKEN_NUMBER,
    TOKEN_PLUS, TOKEN_EQUAL, TOKEN_SEMICOLON,
    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
} Token;

typedef struct {
    const char* start;
    const char* current;
} Lexer;

char advance(Lexer* lexer) { return *lexer->current++; }
char peek(Lexer* lexer) { return *lexer->current; }
bool isAtEnd(Lexer* lexer) { return *lexer->current == '\0'; }

bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

void skipWhitespace(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
            advance(lexer);
        } else {
            break;
        }
    }
}

Token makeToken(Lexer* lexer, TokenType type) {
    return (Token){
        .type = type, 
        .start = lexer->start, 
        .length = (int)(lexer->current - lexer->start)
    };
}

TokenType identifierType(Lexer* lexer) {
    int length = (int)(lexer->current - lexer->start);
    if (length == 3 && memcmp(lexer->start, "int", 3) == 0) return TOKEN_INT;
    return TOKEN_IDENTIFIER;
}

Token nextToken(Lexer* lexer) {
    skipWhitespace(lexer);
    lexer->start = lexer->current;

    if (isAtEnd(lexer)) return makeToken(lexer, TOKEN_EOF);

    char c = advance(lexer);

    if (isAlpha(c)) {
        while (isAlpha(peek(lexer)) || isDigit(peek(lexer))) advance(lexer);
        return makeToken(lexer, identifierType(lexer));
    }
    
    if (isDigit(c)) {
        while (isDigit(peek(lexer))) advance(lexer);
        return makeToken(lexer, TOKEN_NUMBER);
    }

    switch (c) {
        case '+': return makeToken(lexer, TOKEN_PLUS);
        case '=': return makeToken(lexer, TOKEN_EQUAL);
        case ';': return makeToken(lexer, TOKEN_SEMICOLON);
    }

    return (Token){.type = TOKEN_ERROR, .start = "Unknown character", .length = 1};
}

// ==========================================
// 2. PARSER IMPLEMENTATION
// ==========================================

typedef struct {
    Lexer* lexer;
    Token current;
    Token previous;
    bool hadError;
} Parser;

void parserError(Parser* parser, const char* message) {
    if (parser->hadError) return; // Prevent cascading error prints
    parser->hadError = true;
    fprintf(stderr, "Syntax Error at '%.*s': %s\n", 
            parser->current.length, parser->current.start, message);
}

// Advance to the next token
void advanceParser(Parser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = nextToken(parser->lexer);
        if (parser->current.type != TOKEN_ERROR) break;

        parserError(parser, "Lexing error encountered.");
    }
}

// Consume current token if it matches expected type, else throw error
void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advanceParser(parser);
        return;
    }
    parserError(parser, message);
}

bool match(Parser* parser, TokenType type) {
    if (parser->current.type == type) {
        advanceParser(parser);
        return true;
    }
    return false;
}

// Forward declarations for recursive rules
int parseExpression(Parser* parser);

// term = NUMBER | IDENTIFIER
int parseTerm(Parser* parser) {
    if (match(parser, TOKEN_NUMBER)) {
        // Convert token string to integer
        return atoi(parser->previous.start);
    } 
    
    if (match(parser, TOKEN_IDENTIFIER)) {
        printf("[Note: Referenced variable '%.*s']\n", 
               parser->previous.length, parser->previous.start);
        return 0; // Standard dummy value for external variables
    }

    parserError(parser, "Expected number or identifier.");
    return 0;
}

// expression = term ( "+" term )*
int parseExpression(Parser* parser) {
    int value = parseTerm(parser);

    while (match(parser, TOKEN_PLUS)) {
        int right = parseTerm(parser);
        value += right;
    }

    return value;
}

// statement = "int" IDENTIFIER "=" expression ";"
void parseVarDeclaration(Parser* parser) {
    consume(parser, TOKEN_INT, "Expected 'int' keyword.");
    
    Token varName = parser->current;
    consume(parser, TOKEN_IDENTIFIER, "Expected variable name.");

    consume(parser, TOKEN_EQUAL, "Expected '=' after variable name.");

    int val = parseExpression(parser);

    consume(parser, TOKEN_SEMICOLON, "Expected ';' at end of declaration.");

    if (!parser->hadError) {
        printf("Successfully parsed declaration!\n");
        printf("Variable: %.*s | Computed Value: %d\n", 
               varName.length, varName.start, val);
    }
}

// ==========================================
// 3. EXECUTION
// ==========================================

int main() {
    const char* source = "int score = 42 + 100;";
    
    Lexer lexer = { .start = source, .current = source };
    Parser parser = { .lexer = &lexer, .hadError = false };

    // Prime the parser (loads the first token into parser.current)
    advanceParser(&parser);

    // Parse the input
    parseVarDeclaration(&parser);

    return parser.hadError ? EXIT_FAILURE : EXIT_SUCCESS; // or return parser.hadError ? 1 : 0;
}
