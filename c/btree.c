#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define T 3                    // Minimum degree
#define MAX_KEYS (2 * T - 1)   // 5
#define MAX_CHILDREN (2 * T)   // 6
#define INITIAL_CAPACITY 8     // Start small to demonstrate auto-growth!

typedef struct {
    int n;
    int keys[MAX_KEYS];
    uint32_t children[MAX_CHILDREN];
    bool leaf;
} Node;

typedef struct {
    Node *nodes;                    // Dynamic array on the heap
    size_t capacity;                // Current total capacity
    size_t count;                   // Next free index
    uint32_t root;                  // Index of root node
    uint32_t free_list;             // Recycled slots head
} BTree;

// --- Lifecycle Functions ---

bool btree_init(BTree *tree, size_t initial_cap) {
    if (initial_cap < 2) initial_cap = 2; // Need at least slot 0 and slot 1

    tree->nodes = calloc(initial_cap, sizeof(Node));
    if (!tree->nodes) {
        return false;
    }

    tree->capacity = initial_cap;
    tree->count = 1;       // Slot 0 is reserved as NULL
    tree->root = 0;        // 0 means tree is empty
    tree->free_list = 0;
    return true;
}

void btree_destroy(BTree *tree) {
    if (tree && tree->nodes) {
        free(tree->nodes);
        tree->nodes = NULL;
        tree->capacity = 0;
        tree->count = 0;
        tree->root = 0;
        tree->free_list = 0;
    }
}

// --- Dynamic Node Allocation ---

uint32_t allocate_node(BTree *tree, bool leaf) {
    uint32_t idx = 0;

    // 1. Recycle a node from the free list
    if (tree->free_list != 0) {
        idx = tree->free_list;
        tree->free_list = tree->nodes[idx].children[0];
    } 
    // 2. Expand arena if full
    else {
        if (tree->count >= tree->capacity) {
            size_t new_cap = tree->capacity * 2;
            Node *new_nodes = realloc(tree->nodes, new_cap * sizeof(Node));
            if (!new_nodes) {
                fprintf(stderr, "Error: Out of memory during realloc!\n");
                return 0;
            }
            tree->nodes = new_nodes;
            tree->capacity = new_cap;
            printf("  [Arena resized to %zu nodes]\n", new_cap);
        }
        idx = (uint32_t)tree->count++;
    }

    // Reset node values
    memset(&tree->nodes[idx], 0, sizeof(Node));
    tree->nodes[idx].leaf = leaf;
    return idx;
}

void free_node(BTree *tree, uint32_t idx) {
    if (idx == 0 || idx >= tree->count) return;
    tree->nodes[idx].children[0] = tree->free_list;
    tree->free_list = idx;
}

// --- Search ---

bool btree_search(const BTree *tree, uint32_t node_idx, int key) {
    if (node_idx == 0) return false;

    const Node *node = &tree->nodes[node_idx];
    int i = 0;

    while (i < node->n && key > node->keys[i]) {
        i++;
    }

    if (i < node->n && key == node->keys[i]) {
        return true;
    }

    if (node->leaf) {
        return false;
    }

    return btree_search(tree, node->children[i], key);
}

// --- Insertion Operations ---

void btree_split_child(BTree *tree, uint32_t parent_idx, int i, uint32_t child_idx) {
    // 1. Leaf status must be captured before allocation in case realloc moves tree->nodes
    bool child_is_leaf = tree->nodes[child_idx].leaf;

    // 2. Allocate sibling (might trigger realloc!)
    uint32_t sibling_idx = allocate_node(tree, child_is_leaf);
    if (sibling_idx == 0) return;

    // 3. IMPORTANT: Fetch pointers AFTER allocate_node to avoid dangling pointers!
    Node *y = &tree->nodes[child_idx];
    Node *z = &tree->nodes[sibling_idx];

    z->n = T - 1;
    for (int j = 0; j < T - 1; j++) {
        z->keys[j] = y->keys[j + T];
    }

    if (!y->leaf) {
        for (int j = 0; j < T; j++) {
            z->children[j] = y->children[j + T];
        }
    }

    y->n = T - 1;

    // Shift parent children and keys
    Node *p = &tree->nodes[parent_idx];
    for (int j = p->n; j >= i + 1; j--) {
        p->children[j + 1] = p->children[j];
    }
    p->children[i + 1] = sibling_idx;

    for (int j = p->n - 1; j >= i; j--) {
        p->keys[j + 1] = p->keys[j];
    }
    p->keys[i] = y->keys[T - 1];
    p->n++;
}

void btree_insert_non_full(BTree *tree, uint32_t node_idx, int key) {
    int i = tree->nodes[node_idx].n - 1;

    if (tree->nodes[node_idx].leaf) {
        Node *node = &tree->nodes[node_idx];
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->n++;
    } else {
        while (i >= 0 && key < tree->nodes[node_idx].keys[i]) {
            i--;
        }
        i++;

        uint32_t child_idx = tree->nodes[node_idx].children[i];

        if (tree->nodes[child_idx].n == MAX_KEYS) {
            btree_split_child(tree, node_idx, i, child_idx);
            if (key > tree->nodes[node_idx].keys[i]) {
                i++;
            }
        }
        btree_insert_non_full(tree, tree->nodes[node_idx].children[i], key);
    }
}

void btree_insert(BTree *tree, int key) {
    if (tree->root == 0) {
        tree->root = allocate_node(tree, true);
        tree->nodes[tree->root].keys[0] = key;
        tree->nodes[tree->root].n = 1;
        return;
    }

    uint32_t root_idx = tree->root;

    if (tree->nodes[root_idx].n == MAX_KEYS) {
        uint32_t new_root_idx = allocate_node(tree, false);
        tree->root = new_root_idx;
        tree->nodes[new_root_idx].children[0] = root_idx;

        btree_split_child(tree, new_root_idx, 0, root_idx);

        int i = (key > tree->nodes[new_root_idx].keys[0]) ? 1 : 0;
        btree_insert_non_full(tree, tree->nodes[new_root_idx].children[i], key);
    } else {
        btree_insert_non_full(tree, root_idx, key);
    }
}

// --- In-Order Print ---

void btree_print_inorder(const BTree *tree, uint32_t node_idx) {
    if (node_idx == 0) return;
    const Node *node = &tree->nodes[node_idx];
    int i;
    for (i = 0; i < node->n; i++) {
        if (!node->leaf) {
            btree_print_inorder(tree, node->children[i]);
        }
        printf("%d ", node->keys[i]);
    }
    if (!node->leaf) {
        btree_print_inorder(tree, node->children[i]);
    }
}

// --- Main ---

int main(void) {
    BTree tree;
    // Start with a small capacity of 8 nodes to see realloc in action
    if (!btree_init(&tree, INITIAL_CAPACITY)) {
        fprintf(stderr, "Failed to initialize tree\n");
        return 1;
    }

    printf("Inserting keys...\n");
    int values[] = {10, 20, 5, 6, 12, 30, 7, 17, 3, 2, 8, 25, 40, 1, 9, 15, 18, 22, 33};
    int n = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < n; i++) {
        btree_insert(&tree, values[i]);
    }

    printf("\nB-Tree In-order Traversal (Sorted): ");
    btree_print_inorder(&tree, tree.root);
    printf("\n");

    printf("\nTree stats:\n");
    printf("  Total nodes used: %zu\n", tree.count - 1);
    printf("  Final allocated capacity: %zu\n", tree.capacity);

    // Clean up dynamic memory
    btree_destroy(&tree);
    printf("Memory freed successfully.\n");
    return 0;
}
