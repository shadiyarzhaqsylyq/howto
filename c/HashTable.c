// Simple Separate Chaining



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 7

// Linked list node for collision chains
typedef struct Node {
    char *key;
    int value;
    struct Node *next;
} Node;

// Hash table bucket array
typedef struct HashTable {
    Node **buckets;
    int size;
} HashTable;

// String hash function (djb2 algorithm)
unsigned int hash(const char *key, int table_size) {
    unsigned long hash_val = 5381;
    int c;
    while ((c = *key++)) {
        hash_val = ((hash_val << 5) + hash_val) + c; // hash * 33 + c
    }
    return hash_val % table_size;
}

// Initialize a new hash table
HashTable *create_table(int size) {
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    table->size = size;
    table->buckets = (Node **)calloc(size, sizeof(Node *));
    return table;
}

// Create a standalone chain node
Node *create_node(const char *key, int value) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->key = strdup(key); // Allocate and copy string key
    new_node->value = value;
    new_node->next = NULL;
    return new_node;
}

// Insert or update key-value pair
void insert(HashTable *table, const char *key, int value) {
    unsigned int index = hash(key, table->size);
    Node *current = table->buckets[index];

    // Check if key already exists in bucket chain; update if found
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            current->value = value;
            return;
        }
        current = current->next;
    }

    // Key not found: insert at head of list (O(1) time complexity)
    Node *new_node = create_node(key, value);
    new_node->next = table->buckets[index];
    table->buckets[index] = new_node;
}

// Search for a key; returns 1 if found (storing result in *out_value), 0 otherwise
int search(HashTable *table, const char *key, int *out_value) {
    unsigned int index = hash(key, table->size);
    Node *current = table->buckets[index];

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            *out_value = current->value;
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// Delete a entry by key
int delete_key(HashTable *table, const char *key) {
    unsigned int index = hash(key, table->size);
    Node *current = table->buckets[index];
    Node *prev = NULL;

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            if (prev == NULL) {
                table->buckets[index] = current->next; // Remove list head
            } else {
                prev->next = current->next;           // Bypass node
            }
            free(current->key);
            free(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }
    return 0;
}

// Display table state bucket by bucket
void print_table(HashTable *table) {
    printf("\n--- Hash Table State ---\n");
    for (int i = 0; i < table->size; i++) {
        printf("Bucket [%d]: ", i);
        Node *current = table->buckets[i];
        if (!current) {
            printf("NULL\n");
            continue;
        }
        while (current != NULL) {
            printf("(%s: %d) -> ", current->key, current->value);
            current = current->next;
        }
        printf("NULL\n");
    }
}

// Free all dynamically allocated table memory
void free_table(HashTable *table) {
    for (int i = 0; i < table->size; i++) {
        Node *current = table->buckets[i];
        while (current != NULL) {
            Node *temp = current;
            current = current->next;
            free(temp->key);
            free(temp);
        }
    }
    free(table->buckets);
    free(table);
}

int main(void) {
    HashTable *table = create_table(TABLE_SIZE);

    // Insert entries (some will collide into the same bucket)
    insert(table, "apple", 100);
    insert(table, "banana", 200);
    insert(table, "cherry", 300);
    insert(table, "date", 400);
    insert(table, "elderberry", 500);

    print_table(table);

    // Lookup
    int val;
    if (search(table, "banana", &val)) {
        printf("\nLookup 'banana': Found value = %d\n", val);
    }

    // Delete
    printf("\nDeleting key 'banana'...\n");
    delete_key(table, "banana");
    print_table(table);

    // Cleanup memory
    free_table(table);
    printf("\nMemory freed successfully.\n");

    return 0;
}
