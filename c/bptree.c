#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define T 3 // Minimum degree (Max keys per node = 2*T - 1 = 5, Max children = 2*T = 6)

/* ------------------------------------------------------------------ */
/*  B+ Tree Node Structure                                           */
/* ------------------------------------------------------------------ */

typedef struct BPlusNode {
    bool is_leaf;
    int num_keys;
    int keys[2 * T - 1];
    
    // Internal node pointers
    struct BPlusNode *children[2 * T];
    
    // Leaf node pointer (for linked list traversal)
    struct BPlusNode *next;
} BPlusNode;

/* ------------------------------------------------------------------ */
/*  Node Allocation                                                   */
/* ------------------------------------------------------------------ */

BPlusNode *createNode(bool is_leaf) {
    BPlusNode *node = (BPlusNode *)malloc(sizeof(BPlusNode));
    if (!node) {
        perror("Allocation failed");
        exit(EXIT_FAILURE);
    }
    node->is_leaf = is_leaf;
    node->num_keys = 0;
    node->next = NULL;
    for (int i = 0; i < 2 * T; i++) {
        node->children[i] = NULL;
    }
    return node;
}

/* ------------------------------------------------------------------ */
/*  Search                                                            */
/* ------------------------------------------------------------------ */

BPlusNode *search(BPlusNode *root, int key) {
    if (root == NULL) return NULL;

    BPlusNode *curr = root;

    // Traverse down to the leaf node
    while (!curr->is_leaf) {
        int i = 0;
        while (i < curr->num_keys && key >= curr->keys[i]) {
            i++;
        }
        curr = curr->children[i];
    }

    // Search for key within the leaf node
    for (int i = 0; i < curr->num_keys; i++) {
        if (curr->keys[i] == key) {
            return curr; // Found key in leaf
        }
    }

    return NULL; // Key not found
}

/* ------------------------------------------------------------------ */
/*  Split Child Node                                                  */
/* ------------------------------------------------------------------ */

void splitChild(BPlusNode *parent, int i, BPlusNode *child) {
    BPlusNode *z = createNode(child->is_leaf);

    if (child->is_leaf) {
        // --- LEAF NODE SPLIT ---
        // Leaf retains T keys (indices 0 .. T-1)
        // New leaf z gets T-1 keys (indices T .. 2T-2)
        z->num_keys = T - 1;
        for (int j = 0; j < T - 1; j++) {
            z->keys[j] = child->keys[j + T];
        }
        child->num_keys = T;

        // Maintain leaf linked list
        z->next = child->next;
        child->next = z;

        // Routing key pushed to parent is COPIED from z's first key
        int promoted_key = z->keys[0];

        // Make room in parent for new child and key
        for (int j = parent->num_keys; j >= i + 1; j--) {
            parent->children[j + 1] = parent->children[j];
        }
        parent->children[i + 1] = z;

        for (int j = parent->num_keys - 1; j >= i; j--) {
            parent->keys[j + 1] = parent->keys[j];
        }
        parent->keys[i] = promoted_key;
        parent->num_keys++;
    } else {
        // --- INTERNAL NODE SPLIT ---
        // New internal node z gets T-1 keys and T children
        z->num_keys = T - 1;
        for (int j = 0; j < T - 1; j++) {
            z->keys[j] = child->keys[j + T];
        }
        for (int j = 0; j < T; j++) {
            z->children[j] = child->children[j + T];
        }

        // The median key (at index T-1) is PROMOTED (moved) to parent
        int promoted_key = child->keys[T - 1];
        child->num_keys = T - 1;

        // Make room in parent for new child and key
        for (int j = parent->num_keys; j >= i + 1; j--) {
            parent->children[j + 1] = parent->children[j];
        }
        parent->children[i + 1] = z;

        for (int j = parent->num_keys - 1; j >= i; j--) {
            parent->keys[j + 1] = parent->keys[j];
        }
        parent->keys[i] = promoted_key;
        parent->num_keys++;
    }
}

/* ------------------------------------------------------------------ */
/*  Insert into Non-Full Node                                         */
/* ------------------------------------------------------------------ */

void insertNonFull(BPlusNode *node, int key) {
    int i = node->num_keys - 1;

    if (node->is_leaf) {
        // Insert key into sorted leaf node
        while (i >= 0 && node->keys[i] > key) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->num_keys++;
    } else {
        // Find child to navigate to
        while (i >= 0 && key < node->keys[i]) {
            i--;
        }
        i++;

        // Split child if full
        if (node->children[i]->num_keys == 2 * T - 1) {
            splitChild(node, i, node->children[i]);
            if (key >= node->keys[i]) {
                i++;
            }
        }
        insertNonFull(node->children[i], key);
    }
}

/* ------------------------------------------------------------------ */
/*  Public Insert Function                                            */
/* ------------------------------------------------------------------ */

void insert(BPlusNode **rootRef, int key) {
    BPlusNode *root = *rootRef;

    if (root == NULL) {
        *rootRef = createNode(true);
        (*rootRef)->keys[0] = key;
        (*rootRef)->num_keys = 1;
        return;
    }

    // If root is full, split root and grow tree height
    if (root->num_keys == 2 * T - 1) {
        BPlusNode *s = createNode(false);
        s->children[0] = root;
        splitChild(s, 0, root);

        int i = 0;
        if (key >= s->keys[0]) {
            i++;
        }
        insertNonFull(s->children[i], key);
        *rootRef = s;
    } else {
        insertNonFull(root, key);
    }
}

/* ------------------------------------------------------------------ */
/*  Leaf Traversal (Range Scanning)                                  */
/* ------------------------------------------------------------------ */

void printLeafList(BPlusNode *root) {
    if (root == NULL) return;

    // Navigate to the leftmost leaf
    BPlusNode *curr = root;
    while (!curr->is_leaf) {
        curr = curr->children[0];
    }

    // Follow leaf next pointers
    printf("Sequential Leaf List: ");
    while (curr != NULL) {
        printf("[ ");
        for (int i = 0; i < curr->num_keys; i++) {
            printf("%d ", curr->keys[i]);
        }
        printf("]");
        if (curr->next) printf(" -> ");
        curr = curr->next;
    }
    printf("\n");
}

/* ------------------------------------------------------------------ */
/*  Print Tree Structure (Hierarchical)                               */
/* ------------------------------------------------------------------ */

void printTree(BPlusNode *node, int level) {
    if (node == NULL) return;

    printf("Level %d [%s] (%d keys): ", level, node->is_leaf ? "Leaf" : "Internal", node->num_keys);
    for (int i = 0; i < node->num_keys; i++) {
        printf("%d ", node->keys[i]);
    }
    printf("\n");

    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            printTree(node->children[i], level + 1);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Free Memory                                                       */
/* ------------------------------------------------------------------ */

void freeTree(BPlusNode *node) {
    if (node == NULL) return;
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            freeTree(node->children[i]);
        }
    }
    free(node);
}

/* ------------------------------------------------------------------ */
/*  Demo Main                                                         */
/* ------------------------------------------------------------------ */

int main(void) {
    BPlusNode *root = NULL;

    int keys[] = {10, 20, 5, 6, 12, 30, 7, 17, 3, 1, 15, 25, 27};
    int n = (int)(sizeof(keys) / sizeof(keys[0]));

    printf("Inserting keys into B+ Tree...\n\n");
    for (int i = 0; i < n; i++) {
        insert(&root, keys[i]);
    }

    printf("--- Tree Hierarchy ---\n");
    printTree(root, 0);
    printf("\n");

    printf("--- Leaf Linked List Traversal ---\n");
    printLeafList(root);
    printf("\n");

    // Search Demo
    int search_keys[] = {12, 99};
    for (int i = 0; i < 2; i++) {
        int k = search_keys[i];
        BPlusNode *found = search(root, k);
        if (found) {
            printf("Key %d: FOUND in a leaf node.\n", k);
        } else {
            printf("Key %d: NOT FOUND.\n", k);
        }
    }

    freeTree(root);
    return 0;
}
