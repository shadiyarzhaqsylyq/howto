#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#define T 2 // Min degree


typedef struct {
    int is_leaf;// 1 - true, 0 - false
    int  num_keys;
} NodeHeader;

typedef struct LeafNode {
    NodeHeader header;
    int        keys[2 * T - 1];
} LeafNode;

typedef struct InternalNode {
    NodeHeader header;
    int        keys[2 * T - 1];
    void      *children[2 * T];
} InternalNode;

typedef struct RootNode {
    NodeHeader header;
    int        keys[2 * T - 1];
    void      *children[2 * T];
} RootNode;




int main(void) {
    

    int keys[] = {10, 20, 5, 6, 12, 30, 7, 17, 3, 1, 15, 25, 27};
    
    printf("Hello World");


}
