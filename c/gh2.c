#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// --- 1. THE ARENA ALLOCATOR (ZERO MALLOC) ---
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
} Arena;

// Reserve exactly 201 MB of global, static memory at compile time.
// This costs 0 CPU cycles at runtime and bypasses the OS heap entirely.
#define ARENA_STATIC_CAPACITY (201 * 1024 * 1024) 
static uint8_t global_static_buffer[ARENA_STATIC_CAPACITY];

void arena_init_static(Arena *arena) {
    // Points directly to our compile-time reserved memory block.
    // NO MALLOC CALL!
    arena->buffer = global_static_buffer;
    arena->capacity = ARENA_STATIC_CAPACITY;
    arena->offset = 0;
}

void* arena_alloc(Arena *arena, size_t size) {
    size = (size + 7) & ~7; // 8-byte alignment

    if (arena->offset + size > arena->capacity) {
        return NULL; 
    }
    
    void *ptr = &arena->buffer[arena->offset];
    arena->offset += size; 
    return ptr;
}

// --- 2. GENERATIONAL POOL WITH HANDLES ---
typedef struct { uint32_t index; uint32_t generation; } Handle;
typedef struct { float x, y; } Player;

typedef struct {
    Player data;
    uint32_t generation;
    bool is_active;
} Slot;

typedef struct {
    Slot *slots;
    uint32_t *free_slots;
    uint32_t free_count;
    uint32_t max_entities; 
} DatabasePool;

DatabasePool* pool_create_in_arena(Arena *arena, uint32_t max_entities) {
    DatabasePool *pool = arena_alloc(arena, sizeof(DatabasePool));
    if (pool == NULL) return NULL;
    
    pool->slots = arena_alloc(arena, sizeof(Slot) * max_entities);
    pool->free_slots = arena_alloc(arena, sizeof(uint32_t) * max_entities);
    
    if (pool->slots == NULL || pool->free_slots == NULL) {
        fprintf(stderr, "Fatal Error: Static Arena is too small for %u entities!\n", max_entities);
        exit(1);
    }
    
    pool->max_entities = max_entities;
    pool->free_count = max_entities;
    
    for (uint32_t i = 0; i < max_entities; i++) {
        pool->slots[i].generation = 1;
        pool->slots[i].is_active = false;
        pool->free_slots[i] = (max_entities - 1) - i;
    }
    return pool;
}

Handle pool_alloc(DatabasePool *pool, Player data) {
    uint32_t index = pool->free_slots[--pool->free_count];
    Slot *slot = &pool->slots[index];
    slot->data = data;
    slot->is_active = true;
    return (Handle){index, slot->generation};
}

void pool_free(DatabasePool *pool, Handle handle) {
    Slot *slot = &pool->slots[handle.index];
    slot->is_active = false;
    slot->generation++;
    pool->free_slots[pool->free_count++] = handle.index;
}

// --- 3. THE RUNTIME LIFECYCLE ---
int main() {
    uint32_t target_entities = 10000000; // 10 Million

    Arena global_arena;
    // Binds the arena to the global array instantly
    arena_init_static(&global_arena); 

    DatabasePool *pool = pool_create_in_arena(&global_arena, target_entities);
    if (pool == NULL) {
        fprintf(stderr, "Pool creation failed! Arena space exceeded.\n");
        return 1;
    }

    // Allocation and deallocation are now 100% localized stack/array operations
    Handle h1 = pool_alloc(pool, (Player){1.0f, 2.0f});
    pool_free(pool, h1);

    printf("Static Arena offset used: %zu bytes\n", global_arena.offset);
    printf("Static Arena remaining: %zu bytes\n", global_arena.capacity - global_arena.offset);
    
    // NO FREE CALL! 
    // The memory automatically unloads when the OS tears down the process.
    return 0;
}
