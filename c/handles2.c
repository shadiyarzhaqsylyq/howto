#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// 64-bit Struct Handle (32-bit index, 32-bit generation)
typedef struct {
    uint32_t index;
    uint32_t generation;
} Handle;

static const Handle INVALID_HANDLE = {0, 0};

typedef struct {
    char name[32];
    int health;
} Entity;

typedef struct {
    Entity data;
    uint32_t generation;
    bool is_active;
} Slot;

typedef struct {
    Slot* slots;
    uint32_t* free_list; // Stack of available slot indices
    uint32_t capacity;
    uint32_t free_count;
} EntityPool;

// Initialize pool with initial dynamic capacity
bool init_pool(EntityPool* pool, uint32_t initial_capacity) {
    uint32_t cap = initial_capacity < 2 ? 2 : initial_capacity;
    
    pool->slots = (Slot*)calloc(cap, sizeof(Slot));
    pool->free_list = (uint32_t*)malloc(cap * sizeof(uint32_t));
    
    if (!pool->slots || !pool->free_list) {
        free(pool->slots);
        free(pool->free_list);
        return false;
    }

    pool->capacity = cap;
    pool->free_count = 0;

    // Slot 0 is reserved as invalid handle index (Zylinski design pattern)
    pool->slots[0].generation = 0;
    pool->slots[0].is_active = false;

    // Push indices 1 to (cap-1) onto the free-list stack
    for (uint32_t i = cap - 1; i >= 1; i--) {
        pool->slots[i].generation = 1;
        pool->slots[i].is_active = false;
        pool->free_list[pool->free_count++] = i;
    }

    return true;
}

// Grow pool when capacity is exhausted
static bool grow_pool(EntityPool* pool) {
    uint32_t old_cap = pool->capacity;
    uint32_t new_cap = old_cap * 2;

    Slot* new_slots = (Slot*)realloc(pool->slots, new_cap * sizeof(Slot));
    uint32_t* new_free_list = (uint32_t*)realloc(pool->free_list, new_cap * sizeof(uint32_t));

    if (!new_slots || !new_free_list) return false;

    pool->slots = new_slots;
    pool->free_list = new_free_list;
    pool->capacity = new_cap;

    // Add newly allocated slots to the free-list
    for (uint32_t i = new_cap - 1; i >= old_cap; i--) {
        pool->slots[i].generation = 1;
        pool->slots[i].is_active = false;
        pool->free_list[pool->free_count++] = i;
    }

    printf("Pool expanded: %u -> %u capacity\n", old_cap, new_cap);
    return true;
}

// O(1) Allocation
Handle create_entity(EntityPool* pool, const char* name, int health) {
    if (pool->free_count == 0) {
        if (!grow_pool(pool)) return INVALID_HANDLE;
    }

    // Pop available slot index off stack
    uint32_t index = pool->free_list[--pool->free_count];
    Slot* slot = &pool->slots[index];

    slot->is_active = true;
    strncpy(slot->data.name, name, sizeof(slot->data.name) - 1);
    slot->data.name[sizeof(slot->data.name) - 1] = '\0';
    slot->data.health = health;

    return (Handle){ .index = index, .generation = slot->generation };
}

// O(1) Safe Lookup
Entity* get_entity(EntityPool* pool, Handle handle) {
    if (handle.index == 0 || handle.index >= pool->capacity) return NULL;

    Slot* slot = &pool->slots[handle.index];

    // Verification step: Check status & match generation
    if (!slot->is_active || slot->generation != handle.generation) {
        return NULL; // Invalid or stale handle
    }

    return &slot->data;
}

// O(1) Deallocation
bool destroy_entity(EntityPool* pool, Handle handle) {
    Entity* entity = get_entity(pool, handle);
    if (!entity) return false;

    Slot* slot = &pool->slots[handle.index];
    slot->is_active = false;
    slot->generation++; // Invalidate active handles

    // Recycle slot index onto stack
    pool->free_list[pool->free_count++] = handle.index;
    return true;
}

void cleanup_pool(EntityPool* pool) {
    free(pool->slots);
    free(pool->free_list);
    pool->slots = NULL;
    pool->free_list = NULL;
}

int main() {
    EntityPool pool;
    init_pool(&pool, 3); // Start small to demonstrate growing

    Handle h1 = create_entity(&pool, "Orc", 120);
    Handle h2 = create_entity(&pool, "Elf", 80);
    Handle h3 = create_entity(&pool, "Troll", 200); // Triggers pool expansion

    printf("Created Troll at slot index: %u, gen: %u\n", h3.index, h3.generation);

    // Destroy Elf (Slot index 2)
    destroy_entity(&pool, h2);
    printf("Destroyed Elf\n");

    // Stale lookup test
    if (get_entity(&pool, h2) == NULL) {
        printf("Old Elf handle verified STALE.\n");
    }

    // Spawn new entity (Reuses slot index 2, bumps generation)
    Handle h4 = create_entity(&pool, "Mage", 60);
    printf("Created Mage at slot index: %u, gen: %u\n", h4.index, h4.generation);

    cleanup_pool(&pool);
    return 0;
}
