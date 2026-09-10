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
/*
*Buffer Pool using mmap()*

#include <sys/mman.h>
#include <stddef.h>

#define PAGE_SIZE (16 * 1024)       // 16 KB Database Pages
#define POOL_PAGES 1000000          // ~16 GB Buffer Pool

typedef struct {
    char *frames;                   // Raw page data
    size_t total_pages;
} BufferPool;

BufferPool init_buffer_pool() {
    size_t total_bytes = (size_t)PAGE_SIZE * POOL_PAGES;

    // Grab raw memory directly from the OS page table
    void *raw = mmap(NULL, total_bytes, 
                     PROT_READ | PROT_WRITE, 
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    BufferPool pool;
    pool.frames = (char *)raw;
    pool.total_pages = POOL_PAGES;
    return pool;
}

// Sub-allocator access: zero-cost page lookup
char *get_page_frame(BufferPool *pool, size_t page_id) {
    return &pool->frames[page_id * PAGE_SIZE];
}

*Thread Local Arena using malloc() (Startup + Hotpath)*

#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
} ThreadArena;

// AT THREAD SPAWN (Startup):
ThreadArena create_thread_arena(size_t capacity_in_mb) {
    ThreadArena arena;
    arena.capacity = capacity_in_mb * 1024 * 1024;
    arena.buffer = (uint8_t *)malloc(arena.capacity); // Only called ONCE per thread
    arena.offset = 0;
    return arena;
}

// IN TRANSACTION HOT PATH: O(1) Bump Allocation (No OS calls)
void *arena_alloc(ThreadArena *arena, size_t size) {
    // 8-byte alignment
    size_t aligned_offset = (arena->offset + 7) & ~7; 
    
    if (aligned_offset + size > arena->capacity) {
        return NULL; // Handle rare overflow
    }
    
    void *ptr = &arena->buffer[aligned_offset];
    arena->offset = aligned_offset + size;
    return ptr;
}

// ON TRANSACTION COMMIT / ABORT: O(1) Reset
void arena_reset(ThreadArena *arena) {
    arena->offset = 0; // Instantly ready for the next incoming query
}


*/
