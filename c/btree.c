#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define T 3 // Minimum degree (key range: T-1 .. 2T-1)

/* ------------------------------------------------------------------ */
/*  Three distinct node structures                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    bool is_leaf;
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

/* ------------------------------------------------------------------ */
/*  Allocation helpers                                                */
/* ------------------------------------------------------------------ */

static LeafNode *createLeafNode(void) {
    LeafNode *n = (LeafNode *)malloc(sizeof(LeafNode));
    n->header.is_leaf  = true;
    n->header.num_keys = 0;
    return n;
}

static InternalNode *createInternalNode(void) {
    InternalNode *n = (InternalNode *)malloc(sizeof(InternalNode));
    n->header.is_leaf  = false;
    n->header.num_keys = 0;
    for (int i = 0; i < 2 * T; i++)
        n->children[i] = NULL;
    return n;
}

static RootNode *createRootNode(bool is_leaf) {
    RootNode *n = (RootNode *)malloc(sizeof(RootNode));
    n->header.is_leaf  = is_leaf;
    n->header.num_keys = 0;
    for (int i = 0; i < 2 * T; i++)
        n->children[i] = NULL;
    return n;
}

/* ------------------------------------------------------------------ */
/*  Safe accessors                                                    */
/* ------------------------------------------------------------------ */

static inline NodeHeader *hdr(void *node) {
    return (NodeHeader *)node;
}

static inline int *keys_of(void *node) {
    /* keys start immediately after the header in every node type */
    return (int *)((char *)node + sizeof(NodeHeader));
}

static inline void **children_of(void *node) {
    /* only valid for non-leaf nodes; RootNode and InternalNode share layout */
    return ((InternalNode *)node)->children;
}

/* ------------------------------------------------------------------ */
/*  In-order traversal                                                */
/* ------------------------------------------------------------------ */

void traverse(void *node) {
    if (node == NULL) return;

    NodeHeader *h = hdr(node);
    int *keys = keys_of(node);

    if (h->is_leaf) {
        for (int i = 0; i < h->num_keys; i++)
            printf("%d ", keys[i]);
    } else {
        void **ch = children_of(node);
        int i;
        for (i = 0; i < h->num_keys; i++) {
            traverse(ch[i]);
            printf("%d ", keys[i]);
        }
        traverse(ch[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  Search                                                            */
/* ------------------------------------------------------------------ */

void *search(void *root, int k) {
    if (root == NULL) return NULL;

    NodeHeader *h = hdr(root);
    int *keys = keys_of(root);
    int i = 0;

    while (i < h->num_keys && k > keys[i])
        i++;

    if (i < h->num_keys && keys[i] == k)
        return root;

    if (h->is_leaf)
        return NULL;

    return search(children_of(root)[i], k);
}

/* ------------------------------------------------------------------ */
/*  Split a full child y of parent x at index i                       */
/* ------------------------------------------------------------------ */

void splitChild(void *x, int i, void *y) {
    NodeHeader *yh = hdr(y);
    void *z;

    if (yh->is_leaf) {
        LeafNode *yl = (LeafNode *)y;
        LeafNode *zl = createLeafNode();
        zl->header.num_keys = T - 1;
        for (int j = 0; j < T - 1; j++)
            zl->keys[j] = yl->keys[j + T];
        yl->header.num_keys = T - 1;
        z = zl;
    } else {
        InternalNode *yi = (InternalNode *)y;
        InternalNode *zi = createInternalNode();
        zi->header.num_keys = T - 1;
        for (int j = 0; j < T - 1; j++)
            zi->keys[j] = yi->keys[j + T];
        for (int j = 0; j < T; j++)
            zi->children[j] = yi->children[j + T];
        yi->header.num_keys = T - 1;
        z = zi;
    }

    /* parent is always non-leaf */
    NodeHeader *xh = hdr(x);
    int *xkeys = keys_of(x);
    void **xch = children_of(x);

    for (int j = xh->num_keys; j >= i + 1; j--)
        xch[j + 1] = xch[j];
    xch[i + 1] = z;

    for (int j = xh->num_keys - 1; j >= i; j--)
        xkeys[j + 1] = xkeys[j];

    xkeys[i] = keys_of(y)[T - 1];
    xh->num_keys++;
}

/* ------------------------------------------------------------------ */
/*  Insert into a non-full node                                       */
/* ------------------------------------------------------------------ */

void insertNonFull(void *x, int k) {
    NodeHeader *xh = hdr(x);
    int *keys = keys_of(x);
    int i = xh->num_keys - 1;

    if (xh->is_leaf) {
        while (i >= 0 && keys[i] > k) {
            keys[i + 1] = keys[i];
            i--;
        }
        keys[i + 1] = k;
        xh->num_keys++;
    } else {
        while (i >= 0 && keys[i] > k)
            i--;
        i++;

        void **ch = children_of(x);
        if (hdr(ch[i])->num_keys == 2 * T - 1) {
            splitChild(x, i, ch[i]);
            if (keys[i] < k)
                i++;
        }
        insertNonFull(ch[i], k);
    }
}

/* ------------------------------------------------------------------ */
/*  Public insert (may grow the tree at the root)                     */
/* ------------------------------------------------------------------ */

void insert(RootNode **rootRef, int k) {
    RootNode *root = *rootRef;

    if (root == NULL) {
        *rootRef = createRootNode(true);
        keys_of(*rootRef)[0] = k;
        (*rootRef)->header.num_keys = 1;
        return;
    }

    if (root->header.num_keys == 2 * T - 1) {
        RootNode *s = createRootNode(false);
        s->children[0] = root;
        splitChild(s, 0, root);

        int i = 0;
        if (keys_of(s)[0] < k)
            i++;
        insertNonFull(s->children[i], k);
        *rootRef = s;
    } else {
        insertNonFull(root, k);
    }
}


/*  Demo                                                              */


int main(void) {
    RootNode *root = NULL;

    int keys[] = {10, 20, 5, 6, 12, 30, 7, 17, 3, 1, 15, 25, 27};
    int n = (int)(sizeof(keys) / sizeof(keys[0]));

    for (int i = 0; i < n; i++)
        insert(&root, keys[i]);

    printf("In-order traversal of the B-Tree:\n");
    traverse(root);
    printf("\n\n");

    int search_key = 12;
    void *result = search(root, search_key);
    if (result != NULL)
        printf("Key %d found in the tree.\n", search_key);
    else
        printf("Key %d not found in the tree.\n", search_key);

    return 0;
}
