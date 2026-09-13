#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// 1. Core Handle Structure
typedef struct {
    uint32_t idx;
    uint32_t gen;
} Handle;

#define HANDLE_NONE ((Handle){0, 0})

// Helper to compare handles
static inline bool handle_equals(Handle a, Handle b) {
    return (a.idx == b.idx) && (a.gen == b.gen);
}

// 2. Example Payload / Entity Type
typedef struct {
    Handle handle; // Must contain its handle to track its current generation
    int data;
    float pos_x;
    float pos_y;
} Entity;

// 3. Handle Array Container
typedef struct {
    Entity *items;
    size_t capacity;
    size_t count; // Includes the slot 0 dummy item

    uint32_t *freelist;
    size_t freelist_capacity;
    size_t freelist_count;

    size_t num_active;
} EntityArray;

// Initialize the array container
void entity_array_init(EntityArray *ha) {
    ha->items = NULL;
    ha->capacity = 0;
    ha->count = 0;

    ha->freelist = NULL;
    ha->freelist_capacity = 0;
    ha->freelist_count = 0;

    ha->num_active = 0;
}

// Free allocated memory
void entity_array_free(EntityArray *ha) {
    free(ha->items);
    free(ha->freelist);
    entity_array_init(ha);
}

// Add an element (reuses freelist slots if available)
Handle entity_array_add(EntityArray *ha, Entity v) {
    // Re-use a slot from the freelist if available
    if (ha->freelist_count > 0) {
        uint32_t reuse_idx = ha->freelist[--ha->freelist_count];
        Entity *reused = &ha->items[reuse_idx];

        uint32_t next_gen = reused->handle.gen + 1;

        v.handle.idx = reuse_idx;
        v.handle.gen = next_gen;

        *reused = v;
        ha->num_active++;
        return v.handle;
    }

    // Allocate index 0 as a dummy item if empty
    if (ha->count == 0) {
        ha->capacity = 8;
        ha->items = (Entity *)malloc(sizeof(Entity) * ha->capacity);
        memset(&ha->items[0], 0, sizeof(Entity)); // Slot 0 reserved as invalid
        ha->count = 1;
    }

    // Grow array if capacity is reached
    if (ha->count >= ha->capacity) {
        ha->capacity *= 2;
        ha->items = (Entity *)realloc(ha->items, sizeof(Entity) * ha->capacity);
    }

    uint32_t idx = (uint32_t)ha->count++;
    v.handle.idx = idx;
    v.handle.gen = 1; // Generation starts at 1

    ha->items[idx] = v;
    ha->num_active++;

    return v.handle;
}

// Fetch a pointer using a handle (Returns NULL if handle is invalid or stale)
Entity* entity_array_get_ptr(EntityArray *ha, Handle h) {
    if (h.idx == 0 || h.idx >= ha->count) {
        return NULL;
    }

    Entity *slot = &ha->items[h.idx];
    if (handle_equals(slot->handle, h)) {
        return slot;
    }

    return NULL; // Generation mismatch or slot empty
}

// Fetch a copy of the value using a handle
bool entity_array_get(EntityArray *ha, Handle h, Entity *out_val) {
    Entity *ptr = entity_array_get_ptr(ha, h);
    if (ptr != NULL) {
        if (out_val) *out_val = *ptr;
        return true;
    }
    return false;
}

// Remove an item using its handle
void entity_array_remove(EntityArray *ha, Handle h) {
    Entity *slot = entity_array_get_ptr(ha, h);
    if (slot != NULL) {
        // Push slot index to freelist
        if (ha->freelist_count >= ha->freelist_capacity) {
            ha->freelist_capacity = (ha->freelist_capacity == 0) ? 8 : ha->freelist_capacity * 2;
            ha->freelist = (uint32_t *)realloc(ha->freelist, sizeof(uint32_t) * ha->freelist_capacity);
        }
        ha->freelist[ha->freelist_count++] = h.idx;

        // Reset slot data, but preserve generation counter for next reuse
        uint32_t prev_gen = slot->handle.gen;
        memset(slot, 0, sizeof(Entity));
        slot->handle.gen = prev_gen; // Keep generation saved
        slot->handle.idx = 0;        // 0 marks slot as inactive

        ha->num_active--;
    }
}

int main(void) {
    EntityArray entities;
    entity_array_init(&entities);

    // 1. Add entities
    Handle h1 = entity_array_add(&entities, (Entity){ .data = 10, .pos_x = 1.0f });
    Handle h2 = entity_array_add(&entities, (Entity){ .data = 20, .pos_x = 5.0f });
    Handle h3 = entity_array_add(&entities, (Entity){ .data = 30, .pos_x = 9.0f });

    printf("Added 3 entities (h1 idx:%u gen:%u, h2 idx:%u gen:%u)\n", 
            h1.idx, h1.gen, h2.idx, h2.gen);

    // 2. Fetch via pointer and modify
    Entity *e2 = entity_array_get_ptr(&entities, h2);
    if (e2) {
        e2->data = 200;
        printf("Modified h2 data to: %d\n", e2->data);
    }

    // 3. Delete h2
    entity_array_remove(&entities, h2);
    printf("Removed h2\n");

    // Attempting to access deleted handle will fail
    if (entity_array_get_ptr(&entities, h2) == NULL) {
        printf("Success: h2 is now invalid!\n");
    }

    // 4. Reuse slot (h4 will take h2's index, but generation increments to 2)
    Handle h4 = entity_array_add(&entities, (Entity){ .data = 40, .pos_x = 12.0f });
    printf("Added h4 (idx:%u gen:%u)\n", h4.idx, h4.gen);

    // Attempting to use old handle `h2` still safely returns NULL because of generation check
    if (entity_array_get_ptr(&entities, h2) == NULL) {
        printf("Success: Stale handle h2 safely rejected despite index reuse!\n");
    }

    // 5. Iterate over active elements (skip index 0 and holes)
    printf("\n--- Iterating Active Entities ---\n");
    for (size_t i = 1; i < entities.count; ++i) {
        Entity *e = &entities.items[i];
        if (e->handle.idx != 0) { // Check if slot is active
            printf("Slot %zu: data = %d, pos_x = %.1f (gen: %u)\n", 
                    i, e->data, e->pos_x, e->handle.gen);
        }
    }

    // Cleanup
    entity_array_free(&entities);
    return 0;
}
