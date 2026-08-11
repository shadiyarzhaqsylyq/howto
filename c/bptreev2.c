#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define T 3 // Minimum degree

typedef enum { NODE_INTERNAL, NODE_LEAF } NodeType;

/* ------------------------------------------------------------------ */
/*  Node Structures                                                   */
/* ------------------------------------------------------------------ */

// Common header present at the beginning of EVERY node
typedef struct BPlusNode {
    NodeType type;
    int num_keys;
    int keys[2 * T - 1];
} BPlusNode;

// Internal Node: Header + Child Pointers
typedef struct InternalNode {
    BPlusNode header;
    BPlusNode *children[2 * T];
} InternalNode;

// Leaf Node: Header + Next Leaf Pointer (No children pointers wasted!)
typedef struct LeafNode {
    BPlusNode header;
    struct LeafNode *next;
} LeafNode;

/* ------------------------------------------------------------------ */
/*  Node Allocators                                                   */
/* ------------------------------------------------------------------ */

BPlusNode *createLeafNode(void) {
    LeafNode *leaf = (LeafNode *)malloc(sizeof(LeafNode));
    if (!leaf) { perror("Allocation failed"); exit(EXIT_FAILURE); }
    
    leaf->header.type = NODE_LEAF;
    leaf->header.num_keys = 0;
    leaf->next = NULL;
    return (BPlusNode *)leaf;
}

BPlusNode *createInternalNode(void) {
    InternalNode *internal = (InternalNode *)malloc(sizeof(InternalNode));
    if (!internal) { perror("Allocation failed"); exit(EXIT_FAILURE); }
    
    internal->header.type = NODE_INTERNAL;
    internal->header.num_keys = 0;
    for (int i = 0; i < 2 * T; i++) {
        internal->children[i] = NULL;
    }
    return (BPlusNode *)internal;
}

/* ------------------------------------------------------------------ */
/*  Search                                                            */
/* ------------------------------------------------------------------ */

BPlusNode *search(BPlusNode *root, int key) {
    if (root == NULL) return NULL;

    BPlusNode *curr = root;

    // Navigate down internal nodes
    while (curr->type == NODE_INTERNAL) {
        InternalNode *internal = (InternalNode *)curr;
        int i = 0;
        while (i < internal->header.num_keys && key >= internal->header.keys[i]) {
            i++;
        }
        curr = internal->children[i];
    }

    // Search in leaf node
    for (int i = 0; i < curr->num_keys; i++) {
        if (curr->keys[i] == key) {
            return curr;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Split Child Node                                                  */
/* ------------------------------------------------------------------ */

void splitChild(InternalNode *parent, int i, BPlusNode *child) {
    if (child->type == NODE_LEAF) {
        // --- LEAF SPLIT ---
        LeafNode *leaf_child = (LeafNode *)child;
        LeafNode *z = (LeafNode *)createLeafNode();

        z->header.num_keys = T - 1;
        for (int j = 0; j < T - 1; j++) {
            z->header.keys[j] = leaf_child->header.keys[j + T];
        }
        leaf_child->header.num_keys = T;

        // Leaf linked list management
        z->next = leaf_child->next;
        leaf_child->next = z;

        int promoted_key = z->header.keys[0];

        // Shift parent children and keys
        for (int j = parent->header.num_keys; j >= i + 1; j--) {
            parent->children[j + 1] = parent->children[j];
        }
        parent->children[i + 1] = (BPlusNode *)z;

        for (int j = parent->header.num_keys - 1; j >= i; j--) {
            parent->header.keys[j + 1] = parent->header.keys[j];
        }
        parent->header.keys[i] = promoted_key;
        parent->header.num_keys++;

    } else {
        // --- INTERNAL SPLIT ---
        InternalNode *int_child = (InternalNode *)child;
        InternalNode *z = (InternalNode *)createInternalNode();

        z->header.num_keys = T - 1;
        for (int j = 0; j < T - 1; j++) {
            z->header.keys[j] = int_child->header.keys[j + T];
        }
        for (int j = 0; j < T; j++) {
            z->children[j] = int_child->children[j + T];
        }

        int promoted_key = int_child->header.keys[T - 1];
        int_child->header.num_keys = T - 1;

        for (int j = parent->header.num_keys; j >= i + 1; j--) {
            parent->children[j + 1] = parent->children[j];
        }
        parent->children[i + 1] = (BPlusNode *)z;

        for (int j = parent->header.num_keys - 1; j >= i; j--) {
            parent->header.keys[j + 1] = parent->header.keys[j];
        }
        parent->header.keys[i] = promoted_key;
        parent->header.num_keys++;
    }
}

/* ------------------------------------------------------------------ */
/*  Insert non-full & Public Insert                                   */
/* ------------------------------------------------------------------ */

void insertNonFull(BPlusNode *node, int key) {
    if (node->type == NODE_LEAF) {
        int i = node->num_keys - 1;
        while (i >= 0 && node->keys[i] > key) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->num_keys++;
    } else {
        InternalNode *internal = (InternalNode *)node;
        int i = internal->header.num_keys - 1;
        while (i >= 0 && key < internal->header.keys[i]) {
            i--;
        }
        i++;

        if (internal->children[i]->num_keys == 2 * T - 1) {
            splitChild(internal, i, internal->children[i]);
            if (key >= internal->header.keys[i]) {
                i++;
            }
        }
        insertNonFull(internal->children[i], key);
    }
}

void insert(BPlusNode **rootRef, int key) {
    BPlusNode *root = *rootRef;

    if (root == NULL) {
        *rootRef = createLeafNode();
        (*rootRef)->keys[0] = key;
        (*rootRef)->num_keys = 1;
        return;
    }

    if (root->num_keys == 2 * T - 1) {
        InternalNode *s = (InternalNode *)createInternalNode();
        s->children[0] = root;
        splitChild(s, 0, root);

        int i = 0;
        if (key >= s->header.keys[0]) {
            i++;
        }
        insertNonFull(s->children[i], key);
        *rootRef = (BPlusNode *)s;
    } else {
        insertNonFull(root, key);
    }
}

/* ------------------------------------------------------------------ */
/*  Free Memory Helper                                               */
/* ------------------------------------------------------------------ */

void freeTree(BPlusNode *node) {
    if (node == NULL) return;
    if (node->type == NODE_INTERNAL) {
        InternalNode *internal = (InternalNode *)node;
        for (int i = 0; i <= internal->header.num_keys; i++) {
            freeTree(internal->children[i]);
        }
    }
    free(node);
}

/* ------------------------------------------------------------------ */
/*  Demo                                                              */
/* ------------------------------------------------------------------ */

int main(void) {
    BPlusNode *root = NULL;
    int keys[] = {10, 20, 5, 6, 12, 30, 7, 17, 3, 1, 15, 25, 27};
    int n = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < n; i++) {
        insert(&root, keys[i]);
    }

    printf("Search 12: %s\n", search(root, 12) ? "FOUND" : "NOT FOUND");
    printf("Search 99: %s\n", search(root, 99) ? "FOUND" : "NOT FOUND");

    freeTree(root);
    return 0;
}
