#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *buffer;   // Base heap memory from malloc()
    size_t capacity;   // Total size reserved
    size_t offset;     // Next available byte location
} Arena;

// Allocate the backing buffer from the system heap using malloc()
Arena *arena_create(size_t capacity) {
    Arena *arena = (Arena *)malloc(sizeof(Arena));
    if (!arena) return NULL;

    arena->buffer = (uint8_t *)malloc(capacity);
    if (!arena->buffer) {
        free(arena);
        return NULL;
    }

    arena->capacity = capacity;
    arena->offset = 0;
    return arena;
}

// Sub-allocate memory from the arena (Fast bump-pointer)
void *arena_alloc(Arena *arena, size_t size) {
    // 8-byte alignment (standard requirement for multi-byte types)
    size_t aligned_offset = (arena->offset + 7) & ~7;

    if (aligned_offset + size > arena->capacity) {
        return NULL; // Out of arena memory
    }

    void *ptr = &arena->buffer[aligned_offset];
    arena->offset = aligned_offset + size;
    return ptr;
}

// Instantly reuse all arena memory without freeing individual pointers
void arena_reset(Arena *arena) {
    arena->offset = 0;
}

// Free the backing memory back to the OS via free()
void arena_destroy(Arena *arena) {
    if (!arena) return;
    free(arena->buffer);
    free(arena);
}

int main() {
    // Reserve 1 MB of virtual heap memory up-front
    Arena *arena = arena_create(1024 * 1024);

    // High-speed allocations inside the arena chunk
    int *array = (int *)arena_alloc(arena, sizeof(int) * 100);
    char *str = (char *)arena_alloc(arena, 64);

    snprintf(str, 64, "Arena backed by malloc!");
    printf("Message: %s\n", str);
    printf("Allocated bytes: %zu / %zu\n", arena->offset, arena->capacity);

    // Free the entire 1 MB block and control struct in 2 function calls
    arena_destroy(arena);
    return 0;
}
/*
Stack version
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
} Arena;

// Initialize the arena with ANY region of memory
void arena_init(Arena *arena, void *backing_buffer, size_t capacity) {
    arena->buffer = (uint8_t *)backing_buffer;
    arena->capacity = capacity;
    arena->offset = 0;
}

// Sub-allocate from the arena (bumping the offset pointer)
void *arena_alloc(Arena *arena, size_t size) {
    // Simple 8-byte alignment logic
    size_t aligned_offset = (arena->offset + 7) & ~7;
    
    if (aligned_offset + size > arena->capacity) {
        return NULL; // Out of arena memory
    }
    
    void *ptr = &arena->buffer[aligned_offset];
    arena->offset = aligned_offset + size;
    return ptr;
}

// Reset the arena instantaneously without freeing individual pointers
void arena_reset(Arena *arena) {
    arena->offset = 0;
}

int main() {
    // Option A: Backed by Stack Memory (no malloc)
    uint8_t stack_buffer[1024]; 
    
    // Option B: Backed by Static/Global Memory (no malloc)
    // static uint8_t static_buffer[1024 * 1024]; 

    Arena arena;
    arena_init(&arena, stack_buffer, sizeof(stack_buffer));

    // Fast sub-allocations inside the stack-allocated buffer
    int *numbers = (int *)arena_alloc(&arena, sizeof(int) * 10);
    double *values = (double *)arena_alloc(&arena, sizeof(double) * 5);

    for (int i = 0; i < 10; i++) numbers[i] = i * 2;

    printf("Allocated 10 ints and 5 doubles without calling malloc!\n");
    printf("Used %zu / %zu bytes\n", arena.offset, arena.capacity);

    // Free everything at once by resetting the pointer offset
    arena_reset(&arena);
    return 0;
}

*/
