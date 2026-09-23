#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* 1. Temporary AST (only used during parsing)                        */
/* ------------------------------------------------------------------ */

typedef enum {
    NODE_NUMBER,
    NODE_ADD,
    NODE_MUL,
    NODE_VAR
} NodeKind;

typedef struct AstNode {
    NodeKind kind;
    union {
        int value;               /* for NUMBER */
        char name[16];           /* for VAR    */
        struct {
            struct AstNode *left;
            struct AstNode *right;
        } binary;                /* for ADD / MUL */
    };
} AstNode;

/* ------------------------------------------------------------------ */
/* 2. Flat representation (the real IR we keep)                       */
/* ------------------------------------------------------------------ */

typedef enum {
    FLAT_NUMBER,
    FLAT_ADD,
    FLAT_MUL,
    FLAT_VAR
} FlatKind;

typedef struct {
    FlatKind kind;
    int32_t  left;     /* index into nodes[]  (-1 = none) */
    int32_t  right;    /* index into nodes[]  (-1 = none) */
    int32_t  value;    /* for NUMBER */
    char     name[16]; /* for VAR */
} FlatNode;

typedef struct {
    FlatNode *nodes;
    int       count;
    int       capacity;
    int       root;    /* index of the root node */
} FlatAst;

/* ------------------------------------------------------------------ */
/* Helper: grow the flat array                                        */
/* ------------------------------------------------------------------ */
static int flat_push(FlatAst *f, FlatNode n) {
    if (f->count >= f->capacity) {
        f->capacity = f->capacity ? f->capacity * 2 : 16;
        f->nodes = realloc(f->nodes, f->capacity * sizeof(FlatNode));
    }
    f->nodes[f->count] = n;
    return f->count++;
}

/* ------------------------------------------------------------------ */
/* 3. Lower temporary AST → flat arrays (this is the key step)        */
/* ------------------------------------------------------------------ */
static int lower(AstNode *node, FlatAst *f) {
    if (!node) return -1;

    FlatNode n = {0};
    n.left = n.right = -1;

    switch (node->kind) {
    case NODE_NUMBER:
        n.kind  = FLAT_NUMBER;
        n.value = node->value;
        break;

    case NODE_VAR:
        n.kind = FLAT_VAR;
        strncpy(n.name, node->name, sizeof(n.name) - 1);
        break;

    case NODE_ADD:
    case NODE_MUL:
        n.kind  = (node->kind == NODE_ADD) ? FLAT_ADD : FLAT_MUL;
        n.left  = lower(node->binary.left,  f);
        n.right = lower(node->binary.right, f);
        break;
    }

    return flat_push(f, n);
}

/* ------------------------------------------------------------------ */
/* 4. Example: evaluate the flat IR (no more tree walking)            */
/* ------------------------------------------------------------------ */
static int eval_flat(const FlatAst *f, int idx) {
    const FlatNode *n = &f->nodes[idx];

    switch (n->kind) {
    case FLAT_NUMBER: return n->value;
    case FLAT_VAR:    return 42;          /* dummy: all variables = 42 */
    case FLAT_ADD:    return eval_flat(f, n->left) + eval_flat(f, n->right);
    case FLAT_MUL:    return eval_flat(f, n->left) * eval_flat(f, n->right);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Pretty-print the flat representation                               */
/* ------------------------------------------------------------------ */
static void print_flat(const FlatAst *f) {
    printf("Flat IR (%d nodes):\n", f->count);
    for (int i = 0; i < f->count; i++) {
        const FlatNode *n = &f->nodes[i];
        printf("  [%2d] ", i);
        switch (n->kind) {
        case FLAT_NUMBER: printf("NUMBER %d\n", n->value); break;
        case FLAT_VAR:    printf("VAR    %s\n", n->name);  break;
        case FLAT_ADD:    printf("ADD    left=%d  right=%d\n", n->left, n->right); break;
        case FLAT_MUL:    printf("MUL    left=%d  right=%d\n", n->left, n->right); break;
        }
    }
    printf("Root index: %d\n\n", f->root);
}

/* ------------------------------------------------------------------ */
/* Demo                                                               */
/* ------------------------------------------------------------------ */
int main(void) {
    /* Build a temporary tree for:  (3 + x) * 5                       */
    AstNode *x     = malloc(sizeof(AstNode));
    x->kind = NODE_VAR;
    strcpy(x->name, "x");

    AstNode *three = malloc(sizeof(AstNode));
    three->kind  = NODE_NUMBER;
    three->value = 3;

    AstNode *add = malloc(sizeof(AstNode));
    add->kind = NODE_ADD;
    add->binary.left  = three;
    add->binary.right = x;

    AstNode *five = malloc(sizeof(AstNode));
    five->kind  = NODE_NUMBER;
    five->value = 5;

    AstNode *mul = malloc(sizeof(AstNode));
    mul->kind = NODE_MUL;
    mul->binary.left  = add;
    mul->binary.right = five;

    /* ---- Lower to flat IR and throw the tree away ---- */
    FlatAst flat = {0};
    flat.root = lower(mul, &flat);

    /* Free the temporary AST (we never need it again) */
    free(x); free(three); free(add); free(five); free(mul);

    /* Work only with the flat representation from now on */
    print_flat(&flat);

    int result = eval_flat(&flat, flat.root);
    printf("Result of (3 + x) * 5   (x = 42)  →  %d\n", result);

    free(flat.nodes);
    return 0;
}
/*
prints:
[ 0] NUMBER 3
[ 1] VAR    x
[ 2] ADD    left=0  right=1     ←  3 + x
[ 3] NUMBER 5
[ 4] MUL    left=2  right=3     ←  (3 + x) * 5

ADD(node[0], node[1])
MUL(node[2], node[3])

*/
