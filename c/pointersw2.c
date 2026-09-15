#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODES 10

/* In-memory node with real pointers */
typedef struct Node {
    int data;
    struct Node *next;
} Node;

/* Disk / serialized form (unswizzled) */
typedef struct {
    int data;
    unsigned int id;
    unsigned int next_id;   /* 0 means NULL */
} SavedNode;

/* ---------- Helper: build a simple list ---------- */
Node *create_list(void) {
    Node *n1 = malloc(sizeof(Node));
    Node *n2 = malloc(sizeof(Node));
    Node *n3 = malloc(sizeof(Node));

    n1->data = 10;  n1->next = n2;
    n2->data = 20;  n2->next = n3;
    n3->data = 30;  n3->next = NULL;

    return n1;
}

/* ---------- Unswizzle: convert pointers → IDs ---------- */
int unswizzle(Node *head, SavedNode *out, int max) {
    Node *cur = head;
    int count = 0;

    /* First pass: assign IDs and store data */
    while (cur && count < max) {
        out[count].data = cur->data;
        out[count].id   = count + 1;          /* IDs start at 1 */
        out[count].next_id = 0;               /* filled later */
        cur = cur->next;
        count++;
    }

    /* Second pass: fill next_id using the IDs we just assigned */
    cur = head;
    for (int i = 0; i < count; i++) {
        if (cur->next) {
            /* Find the ID of the next node */
            for (int j = 0; j < count; j++) {
                /* In a real system you would use a map; here we just search */
                if (out[j].data == cur->next->data) {  /* simple match for demo */
                    out[i].next_id = out[j].id;
                    break;
                }
            }
        }
        cur = cur->next;
    }
    return count;
}

/* ---------- Swizzle: convert IDs → real pointers ---------- */
Node *swizzle(SavedNode *saved, int count) {
    Node *nodes[MAX_NODES] = {0};

    /* Allocate all nodes first */
    for (int i = 0; i < count; i++) {
        nodes[i] = malloc(sizeof(Node));
        nodes[i]->data = saved[i].data;
        nodes[i]->next = NULL;
    }

    /* Now turn next_id into real pointers */
    for (int i = 0; i < count; i++) {
        if (saved[i].next_id != 0) {
            /* ID starts at 1, so index = id-1 */
            nodes[i]->next = nodes[saved[i].next_id - 1];
        }
    }

    return nodes[0];   /* return the head */
}

/* ---------- Utility ---------- */
void print_list(Node *head) {
    while (head) {
        printf("%d -> ", head->data);
        head = head->next;
    }
    printf("NULL\n");
}

void free_list(Node *head) {
    while (head) {
        Node *tmp = head;
        head = head->next;
        free(tmp);
    }
}

int main(void) {
    /* 1. Create a normal list */
    Node *original = create_list();
    printf("Original list: ");
    print_list(original);

    /* 2. Unswizzle (prepare for saving) */
    SavedNode saved[MAX_NODES];
    int n = unswizzle(original, saved, MAX_NODES);

    printf("\nUnswizzled (saved) form:\n");
    for (int i = 0; i < n; i++) {
        printf("  id=%u  data=%d  next_id=%u\n",
               saved[i].id, saved[i].data, saved[i].next_id);
    }

    /* 3. (Imagine we wrote 'saved' to a file and later read it back) */

    /* 4. Swizzle: turn IDs back into real pointers */
    Node *restored = swizzle(saved, n);
    printf("\nAfter swizzling: ");
    print_list(restored);

    free_list(original);
    free_list(restored);
    return 0;
}
