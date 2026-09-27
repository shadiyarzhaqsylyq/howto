#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define BPTREE_ORDER 8
#define MAX_KEYS     (BPTREE_ORDER - 1)

typedef int64_t  bkey_t;
typedef uint64_t bval_t;

/* ========================================================================= */
/* C99 Compliant Node Definition (Named Union)                               */
/* ========================================================================= */

typedef struct BPTNode {
    bool is_leaf;
    int num_keys;
    struct BPTNode *parent;
    bkey_t keys[BPTREE_ORDER];

    // Explicitly named union for ISO C99 compliance
    union {
        struct BPTNode *children[BPTREE_ORDER + 1];
        struct {
            bval_t values[BPTREE_ORDER];
            struct BPTNode *next;
            struct BPTNode *prev;
        } leaf;
    } sub;
} BPTNode;

typedef struct {
    BPTNode *root;
    size_t size;
} BPTree;

/* ========================================================================= */
/* Allocation & Deallocation                                                 */
/* ========================================================================= */

static BPTNode* node_create(bool is_leaf) {
    BPTNode *node = (BPTNode *)calloc(1, sizeof(BPTNode));
    if (!node) {
        perror("node_create failed");
        exit(EXIT_FAILURE);
    }
    node->is_leaf = is_leaf;
    node->num_keys = 0;
    node->parent = NULL;
    if (is_leaf) {
        node->sub.leaf.next = NULL;
        node->sub.leaf.prev = NULL;
    }
    return node;
}

BPTree* bptree_create(void) {
    BPTree *tree = (BPTree *)malloc(sizeof(BPTree));
    if (!tree) {
        perror("bptree_create failed");
        exit(EXIT_FAILURE);
    }
    tree->root = NULL;
    tree->size = 0;
    return tree;
}

static void node_destroy_recursive(BPTNode *node) {
    int i;
    if (!node) return;
    if (!node->is_leaf) {
        for (i = 0; i <= node->num_keys; i++) {
            node_destroy_recursive(node->sub.children[i]);
        }
    }
    free(node);
}

void bptree_destroy(BPTree *tree) {
    if (!tree) return;
    node_destroy_recursive(tree->root);
    free(tree);
}

/* ========================================================================= */
/* Lookups & Navigation                                                      */
/* ========================================================================= */

static int leaf_lower_bound(const BPTNode *node, bkey_t key) {
    int low = 0, high = node->num_keys;
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (node->keys[mid] < key) low = mid + 1;
        else high = mid;
    }
    return low;
}

static int internal_find_child(const BPTNode *node, bkey_t key) {
    int low = 0, high = node->num_keys - 1;
    int child_idx = 0;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (node->keys[mid] <= key) {
            child_idx = mid + 1;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return child_idx;
}

static BPTNode* find_leaf(BPTNode *root, bkey_t key) {
    BPTNode *curr = root;
    if (!curr) return NULL;
    while (!curr->is_leaf) {
        int idx = internal_find_child(curr, key);
        curr = curr->sub.children[idx];
    }
    return curr;
}

bool bptree_get(BPTree *tree, bkey_t key, bval_t *out_val) {
    BPTNode *leaf;
    int idx;
    if (!tree || !tree->root) return false;

    leaf = find_leaf(tree->root, key);
    idx = leaf_lower_bound(leaf, key);
    if (idx < leaf->num_keys && leaf->keys[idx] == key) {
        if (out_val) *out_val = leaf->sub.leaf.values[idx];
        return true;
    }
    return false;
}

void bptree_range_scan(BPTree *tree, bkey_t start_key, bkey_t end_key,
                       void (*callback)(bkey_t key, bval_t val, void *ctx), void *ctx) {
    BPTNode *leaf;
    int idx;

    if (!tree || !tree->root) return;

    leaf = find_leaf(tree->root, start_key);
    idx = leaf_lower_bound(leaf, start_key);

    while (leaf) {
        for (int i = idx; i < leaf->num_keys; i++) {
            if (leaf->keys[i] > end_key) return;
            callback(leaf->keys[i], leaf->sub.leaf.values[i], ctx);
        }
        leaf = leaf->sub.leaf.next;
        idx = 0;
    }
}

/* ========================================================================= */
/* Insertion & Splitting                                                     */
/* ========================================================================= */

static void insert_into_parent(BPTree *tree, BPTNode *left, bkey_t key, BPTNode *right);

static void split_leaf(BPTree *tree, BPTNode *leaf) {
    int mid = leaf->num_keys / 2;
    int r_keys = leaf->num_keys - mid;
    BPTNode *right = node_create(true);
    right->parent = leaf->parent;

    for (int i = 0; i < r_keys; i++) {
        right->keys[i] = leaf->keys[mid + i];
        right->sub.leaf.values[i] = leaf->sub.leaf.values[mid + i];
    }
    right->num_keys = r_keys;
    leaf->num_keys = mid;

    right->sub.leaf.next = leaf->sub.leaf.next;
    right->sub.leaf.prev = leaf;
    if (leaf->sub.leaf.next) {
        leaf->sub.leaf.next->sub.leaf.prev = right;
    }
    leaf->sub.leaf.next = right;

    insert_into_parent(tree, leaf, right->keys[0], right);
}

static void split_internal(BPTree *tree, BPTNode *node) {
    int mid = node->num_keys / 2;
    bkey_t promoted_key = node->keys[mid];
    int r_keys = node->num_keys - (mid + 1);

    BPTNode *right = node_create(false);
    right->parent = node->parent;

    for (int i = 0; i < r_keys; i++) {
        right->keys[i] = node->keys[mid + 1 + i];
    }
    for (int i = 0; i <= r_keys; i++) {
        right->sub.children[i] = node->sub.children[mid + 1 + i];
        right->sub.children[i]->parent = right;
    }

    right->num_keys = r_keys;
    node->num_keys = mid;

    insert_into_parent(tree, node, promoted_key, right);
}

static void insert_into_parent(BPTree *tree, BPTNode *left, bkey_t key, BPTNode *right) {
    BPTNode *parent = left->parent;

    if (!parent) {
        BPTNode *new_root = node_create(false);
        new_root->keys[0] = key;
        new_root->sub.children[0] = left;
        new_root->sub.children[1] = right;
        new_root->num_keys = 1;

        left->parent = new_root;
        right->parent = new_root;
        tree->root = new_root;
        return;
    }

    int child_idx = 0;
    while (child_idx <= parent->num_keys && parent->sub.children[child_idx] != left) {
        child_idx++;
    }

    for (int i = parent->num_keys; i > child_idx; i--) {
        parent->keys[i] = parent->keys[i - 1];
    }
    for (int i = parent->num_keys + 1; i > child_idx + 1; i--) {
        parent->sub.children[i] = parent->sub.children[i - 1];
    }

    parent->keys[child_idx] = key;
    parent->sub.children[child_idx + 1] = right;
    right->parent = parent;
    parent->num_keys++;

    if (parent->num_keys > MAX_KEYS) {
        split_internal(tree, parent);
    }
}

bool bptree_insert(BPTree *tree, bkey_t key, bval_t value) {
    BPTNode *leaf;
    int idx;

    if (!tree) return false;

    if (!tree->root) {
        tree->root = node_create(true);
        tree->root->keys[0] = key;
        tree->root->sub.leaf.values[0] = value;
        tree->root->num_keys = 1;
        tree->size++;
        return true;
    }

    leaf = find_leaf(tree->root, key);
    idx = leaf_lower_bound(leaf, key);

    if (idx < leaf->num_keys && leaf->keys[idx] == key) {
        leaf->sub.leaf.values[idx] = value;
        return true;
    }

    for (int i = leaf->num_keys; i > idx; i--) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->sub.leaf.values[i] = leaf->sub.leaf.values[i - 1];
    }

    leaf->keys[idx] = key;
    leaf->sub.leaf.values[idx] = value;
    leaf->num_keys++;
    tree->size++;

    if (leaf->num_keys > MAX_KEYS) {
        split_leaf(tree, leaf);
    }

    return true;
}

/* ========================================================================= */
/* Deletion                                                                  */
/* ========================================================================= */

bool bptree_delete(BPTree *tree, bkey_t key) {
    BPTNode *leaf;
    int idx;

    if (!tree || !tree->root) return false;

    leaf = find_leaf(tree->root, key);
    idx = leaf_lower_bound(leaf, key);

    if (idx >= leaf->num_keys || leaf->keys[idx] != key) {
        return false;
    }

    for (int i = idx; i < leaf->num_keys - 1; i++) {
        leaf->keys[i] = leaf->keys[i + 1];
        leaf->sub.leaf.values[i] = leaf->sub.leaf.values[i + 1];
    }
    leaf->num_keys--;
    tree->size--;

    if (leaf == tree->root && leaf->num_keys == 0) {
        free(leaf);
        tree->root = NULL;
    }

    return true;
}

/* ========================================================================= */
/* Verification                                                              */
/* ========================================================================= */

static void print_item(bkey_t key, bval_t val, void *ctx) {
    (void)ctx;
    printf("   [Key: %3ld, RID: %lu]\n", key, val);
}

int main(void) {
    BPTree *tree = bptree_create();
    const int TOTAL_KEYS = 500;
    bval_t val;
    bkey_t probes[] = {10, 2500, 5000, 999};

    for (int i = 1; i <= TOTAL_KEYS; i++) {
        bptree_insert(tree, i * 10, 9000 + i);
    }

    printf("Total keys in index: %zu\n", tree->size);

    for (size_t i = 0; i < sizeof(probes)/sizeof(probes[0]); i++) {
        if (bptree_get(tree, probes[i], &val)) {
            printf("Key %4ld -> FOUND (RID: %lu)\n", probes[i], val);
        } else {
            printf("Key %4ld -> NOT FOUND\n", probes[i]);
        }
    }

    printf("\nRange query [2480 .. 2530]:\n");
    bptree_range_scan(tree, 2480, 2530, print_item, NULL);

    bptree_delete(tree, 2500);
    printf("\nAfter deleting key 2500: %s\n", 
           bptree_get(tree, 2500, &val) ? "FOUND" : "NOT FOUND");

    bptree_destroy(tree);
    return 0;
}
