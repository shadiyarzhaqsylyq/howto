#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
} Arena;

typedef struct { uint32_t index; uint32_t generation; } Handle;
typedef struct { float x, y; } Player;


typedef struct {
    union {
        Player data;         // Active data (8 bytes)
        uint32_t next_free;  // Free list link (4 bytes) - used ONLY when is_active == false
    };
    uint32_t generation;     // Safe generation token
    bool is_active;
} Slot;



typedef struct {
    Slot *slots;
    uint32_t free_head;     // Points to the first available index
    uint32_t max_entities; 
    uint32_t active_count;   // Optional: Track total alive items
} DatabasePool;

void arena_init(Arena *arena, size_t capacity) {
    arena->buffer = malloc(capacity);
    // GUARD 1: Check if the OS actually gave us the 200MB+ buffer
    if (arena->buffer == NULL) {
        fprintf(stderr, "Fatal Error: OS refused to allocate %zu bytes!\n", capacity);
        exit(1);
    }
    arena->capacity = capacity;
    arena->offset = 0;
}

void* arena_alloc(Arena *arena, size_t size) {
    // ALIGNMENT FIX: Round size up to nearest 8 bytes to prevent hardware alignment faults
    size = (size + 7) & ~7; 

    // GUARD 2: Protect against running out of space inside the arena
    if (arena->offset + size > arena->capacity) {
        return NULL; 
    }
    
    void *ptr = &arena->buffer[arena->offset];
    arena->offset += size; 
    return ptr;
}


DatabasePool* pool_create_in_arena(Arena *arena, uint32_t max_entities) {
    DatabasePool *pool = arena_alloc(arena, sizeof(DatabasePool));
    if (pool == NULL) exit(1);
    
    pool->slots = arena_alloc(arena, sizeof(Slot) * max_entities);
    if (pool->slots == NULL) exit(1);
    
    pool->max_entities = max_entities;
    pool->active_count = 0;
    pool->free_head = 0; // The chain starts at index 0
    
    // Chain all slots together into a linked-list using their inner indexes
    for (uint32_t i = 0; i < max_entities; i++) {
        pool->slots[i].generation = 1;
        pool->slots[i].is_active = false;
        pool->slots[i].next_free = i + 1; // Direct link to the next slot
    }
    // Mark the last slot as the end of the list
    pool->slots[max_entities - 1].next_free = UINT32_MAX; 
    
    return pool;
}


Handle pool_alloc(DatabasePool *pool, Player data) {
    // GUARD: Check if the free list is exhausted
    if (pool->free_head == UINT32_MAX) {
        fprintf(stderr, "Warning: Pool overflow!\n");
        return (Handle){0, 0};
    }

    uint32_t index = pool->free_head;
    Slot *slot = &pool->slots[index];
    
    // Pop from the free list chain
    pool->free_head = slot->next_free;
    
    // Initialize object data
    slot->data = data;
    slot->is_active = true;
    pool->active_count++;
    
    return (Handle){index, slot->generation};
}



void pool_free(DatabasePool *pool, Handle handle) {
    if (handle.index >= pool->max_entities) return;

    Slot *slot = &pool->slots[handle.index];
    
    // GUARD: Stale handle protection
    if (!slot->is_active || slot->generation != handle.generation) return;

    slot->is_active = false;
    slot->generation++; // Kill any existing external handles
    
    // Push this slot to the front of the free list chain
    slot->next_free = pool->free_head;
    pool->free_head = handle.index;
    
    pool->active_count--;
}

int main() {
    uint32_t target_entities = 10000000; // 10 Million

    size_t pool_size = sizeof(DatabasePool) + 
                       (sizeof(Slot) * target_entities) + 
                       (sizeof(uint32_t) * target_entities);
                       
    // FIX: Give it 1MB (1024 * 1024) of padding headroom instead of just 2KB.
    // This safely absorbs any 8-byte CPU alignment padding shifts.
    size_t arena_bytes_needed = pool_size + (1024 * 1024); 

    Arena global_arena;
    arena_init(&global_arena, arena_bytes_needed); 

    DatabasePool *pool = pool_create_in_arena(&global_arena, target_entities);
    
    // Safety check in main
    if (pool == NULL) {
        fprintf(stderr, "Pool creation failed!\n");
        free(global_arena.buffer);
        return 1;
    }

    // Test allocation
    Handle h1 = pool_alloc(pool, (Player){1.0f, 2.0f});
    pool_free(pool, h1);

    printf("Arena offset used: %zu bytes\n", global_arena.offset);
    printf("Arena total capacity: %zu bytes\n", global_arena.capacity);
    
    free(global_arena.buffer); 
    return 0;
}

