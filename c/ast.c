#include <stdio.h>

// ## METHOD ##
/*
#define MAX_AST_NODES 1000000 // 1 Million Nodes!



struct ASTNode {
    enum {
        AST_NUMBER,
        AST_ADD,
        AST_MUL,
    } tag;
    union {
        struct AST_NUMBER { int number; } AST_NUMBER;
        struct AST_ADD { ASTHandle left; ASTHandle right; } AST_ADD;
        struct AST_MUL { ASTHandle left; ASTHandle right; } AST_MUL;
    } data;
};

typedef struct { uint32_t id; } ASTHandle;

typedef struct {
    // Array 1: Pure data. No tracking headers, no metadata.
    ASTNode nodes[MAX_AST_NODES];       
    
    // Array 2: Parallel tracking arrays for safety and free-list management
    uint8_t  generations[MAX_AST_NODES]; 
    bool     is_live[MAX_AST_NODES];     
    uint32_t next_free[MAX_AST_NODES];   

    uint32_t free_head;           
    uint32_t len;             
} ASTPool;





*/



typedef enum {
    AST_LITERAL,
    AST_PLUS,
    AST_MULTIPLY,

} AstNodeType;

typedef struct _ast{
    AstNodeType type;
    float value;
    struct _ast *left;
    struct _ast *right;
} AstNode;

void
ast_print(AstNode *ast, int level){

for(int i = 0; i < level; i++){
printf("  ");
}

switch(ast->type){
case AST_LITERAL:
    printf("%.2f\n", ast->value);
    break;
case AST_PLUS:
    printf("+\n");
    ast_print(ast->left, level + 1);
    ast_print(ast->right, level + 1);
    break;
case AST_MULTIPLY:
    printf("*\n");
    ast_print(ast->left, level + 1);
    ast_print(ast->right, level + 1);
    break;

}

}

float
ast_evaluate(AstNode *ast){
switch(ast->type){
case AST_LITERAL:
    return ast->value;
case AST_PLUS: {
    float a = ast_evaluate(ast->left);
    float b = ast_evaluate(ast->right);

    return a + b;
}

    case AST_MULTIPLY: {
        float a = ast_evaluate(ast->left);
        float b = ast_evaluate(ast->right);

        return a * b;
    }

}//switch

}//funcend

int
main()
{
    AstNode one = { .type = AST_LITERAL, .value = 1.0f };
    AstNode two = { .type = AST_LITERAL, .value = 2.0f };
    AstNode three = { .type = AST_LITERAL, .value = 3.0f };

    AstNode plus1 = { .type = AST_PLUS, .left = &one, .right = &two};
    AstNode example1 = { .type = AST_MULTIPLY, .left = &plus1, .right = &three};

    AstNode mul1 = { .type = AST_MULTIPLY, .left = &two, .right = &three};
    AstNode example2 = { .type = AST_PLUS, .left = &one, .right = &mul1};

    float result1 = ast_evaluate(&example1);
    float result2 = ast_evaluate(&example2);

    ast_print(&example1, 2);
    printf("Example 1: %.2f\n", result1);

    ast_print(&example2, 2);
    printf("Example 2: %.2f\n", result2);

    return 0;
}
