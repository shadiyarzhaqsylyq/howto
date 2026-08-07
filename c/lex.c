#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// 1. Define the structural token types
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

// 2. Define the Lexer state tracking structure
typedef struct {
    const char* start;
    const char* current;
} Lexer;

// Core Helper Functions
char advance(Lexer* lexer) { return *lexer->current++; }
char peek(Lexer* lexer) { return *lexer->current; }
bool isAtEnd(Lexer* lexer) { return *lexer->current == '\0'; }

bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Skip formatting whitespace characters inline
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

// Helper to bundle and ship out matched tokens
Token makeToken(Lexer* lexer, TokenType type) {
    return (Token){
        .type = type, 
        .start = lexer->start, 
        .length = (int)(lexer->current - lexer->start)
    };
}

// 3. Hand-written Keyword Matching (Mini-Trie structure)
TokenType identifierType(Lexer* lexer) {
    int length = (int)(lexer->current - lexer->start);
    if (length == 3 && memcmp(lexer->start, "int", 3) == 0) return TOKEN_INT;
    return TOKEN_IDENTIFIER;
}

// 4. The Main Loop (The State Machine)
Token nextToken(Lexer* lexer) {
    skipWhitespace(lexer);
    lexer->start = lexer->current;

    if (isAtEnd(lexer)) return makeToken(lexer, TOKEN_EOF);

    char c = advance(lexer);

    // Hand-written rule routing based on properties
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

// 5. Test execution
int main() {
    const char* source = "int score = 42 + 100;";
    Lexer lexer = { .start = source, .current = source };

    printf("Source: %s\n\n", source);
    
    for (;;) {
        Token token = nextToken(&lexer);
        
        // Pretty print results
        if (token.type == TOKEN_EOF) {
            printf("[EOF]\n");
            break;
        } else if (token.type == TOKEN_ERROR) {
            printf("Lexing Error encountered.\n");
            break;
        } else {
            printf("Token: %-12d Text: '%.*s'\n", token.type, token.length, token.start);
        }
    }
    return 0;
}
//### Output ###
/*
Token: 0            Text: 'int'
Token: 1            Text: 'score'
Token: 4            Text: '='
Token: 2            Text: '42'
Token: 3            Text: '+'
Token: 2            Text: '100'
Token: 5            Text: ';'
[EOF]


*/
