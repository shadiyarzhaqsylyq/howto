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
