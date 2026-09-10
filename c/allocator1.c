#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
} Arena;

// 1. Initialize by grabbing a large chunk
Arena arena_create(size_t capacity) {
    Arena a;
    a.buffer = (uint8_t *)malloc(capacity); // or mmap/VirtualAlloc
    // Linux 2MB Huge Pages allocation for the Buffer Pool
    //void *buffer_pool = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    //mprotect(ptr, size, PROT_READ | PROT_WRITE)
    a.capacity = capacity;
    a.offset = 0;
    return a;
}

// 2. Allocate by bumping the offset (aligned to 8 bytes)
void *arena_alloc(Arena *a, size_t size) {
    // Align offset to 8 bytes (or alignof(max_align_t))
    size_t aligned_offset = (a->offset + 7) & ~7;
    
    if (aligned_offset + size > a->capacity) {
        return NULL; // Out of memory
    }
    
    void *ptr = &a->buffer[aligned_offset];
    a->offset = aligned_offset + size;
    return ptr;
}

// 3. Reset/Reuse everything in O(1)
void arena_reset(Arena *a) {
    a->offset = 0; // The entire block is instantly available again
}

// 4. Destroy when done with the entire system
void arena_free(Arena *a) {
    free(a->buffer);
    a->buffer = NULL;
    a->capacity = 0;
    a->offset = 0;
}
