#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Macro to align memory sizes to the nearest multiple of the machine's word size (usually 8 bytes)
#define ALIGN_UP(size, alignment) (((size) + ((alignment) - 1)) & ~((alignment) - 1))
#define DEFAULT_ALIGNMENT sizeof(void*)

// 1. Define the Arena Structure
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
} Arena;

// 2. Initialize the Arena using a single malloc()
Arena arena_create(size_t capacity) {
    Arena arena = {0};
    arena.buffer = (uint8_t *)malloc(capacity);
    if (arena.buffer == NULL) {
        perror("Failed to allocate arena backing buffer");
        exit(EXIT_FAILURE);
    }
    arena.capacity = capacity;
    arena.offset = 0;
    return arena;
}
// better
void* arena_alloc(Arena *arena, size_t size) {
    // 1. Calculate the current absolute memory address
    uintptr_t current_ptr = (uintptr_t)&arena->buffer[arena->offset];
    
    // 2. Align the absolute pointer up to the required alignment boundary
    uintptr_t aligned_ptr = ALIGN_UP(current_ptr, DEFAULT_ALIGNMENT);
    
    // 3. Calculate the new offset based on the aligned pointer position + requested size
    size_t new_offset = (aligned_ptr - (uintptr_t)arena->buffer) + size;
    
    // Check for out-of-memory
    if (new_offset > arena->capacity) {
        printf("Arena Out of Memory!\n");
        return NULL;
    }
    
    // 4. Update the offset and return the cleanly aligned pointer
    arena->offset = new_offset;
    return (void*)aligned_ptr;
}


// 3. Allocate memory from the Arena with automatic alignment
void* arena_alloc(Arena *arena, size_t size) {
    size_t aligned_size = ALIGN_UP(size, DEFAULT_ALIGNMENT);
    
    // Check for out-of-memory
    if (arena->offset + aligned_size > arena->capacity) {
        printf("Arena Out of Memory!\n");
        return NULL;
    }
    
    // Grab the pointer at the current offset
    void *ptr = &arena->buffer[arena->offset];
    
    // Move the offset forward for the next allocation
    arena->offset += aligned_size;
    return ptr;
}

// 4. Reset the Arena to instantly "free" and reuse all memory
void arena_reset(Arena *arena) {
    arena->offset = 0;
}

// 5. Clean up the base malloc allocation at the very end of the program
void arena_destroy(Arena *arena) {
    free(arena->buffer);
    arena->buffer = NULL;
    arena->capacity = 0;
    arena->offset = 0;
}

// Define a sample struct to test reuse
typedef struct {
    int id;
    double value;
} Player;

int main() {
    // Reserve a 1 KB pool of memory upfront
    Arena my_arena = arena_create(1024);
    printf("--- Arena Initialized (Capacity: %zu bytes) ---\n\n", my_arena.capacity);

    // ==========================================
    // PHASE 1: Allocate and use an int array
    // ==========================================
    int *int_arr = (int *)arena_alloc(&my_arena, 5 * sizeof(int));
    printf("Allocated int array at memory address: %p\n", (void*)int_arr);
    printf("Arena offset is now: %zu bytes\n", my_arena.offset);

    for (int i = 0; i < 5; i++) {
        int_arr[i] = (i + 1) * 10;
        printf("int_arr[%d] = %d\n", i, int_arr[i]);
    }

    // ==========================================
    // PHASE 2: Reset the Arena (Instant Reuse)
    // ==========================================
    printf("\n--- Resetting Arena (No free() called on the int pointer) ---\n");
    arena_reset(&my_arena);
    printf("Arena offset reset to: %zu bytes\n\n", my_arena.offset);

    // ==========================================
    // PHASE 3: Overwrite the memory with structs
    // ==========================================
    Player *player_arr = (Player *)arena_alloc(&my_arena, 3 * sizeof(Player));
    printf("Allocated Player struct array at address: %p\n", (void*)player_arr);
    printf("Arena offset is now: %zu bytes\n", my_arena.offset);

    // Notice that the memory address is exactly the same!
    if ((void*)int_arr == (void*)player_arr) {
        printf("-> Success! The exact same memory location was reused.\n");
    }

    // Populate and print the structs to prove it works seamlessly
    player_arr[0] = (Player){.id = 99, .value = 45.67};
    player_arr[1] = (Player){.id = 100, .value = 89.12};
    
    printf("player_arr[0] -> ID: %d, Val: %.2f\n", player_arr[0].id, player_arr[0].value);
    printf("player_arr[1] -> ID: %d, Val: %.2f\n", player_arr[1].id, player_arr[1].value);

    // Clean up the entire 1KB buffer before exiting
    arena_destroy(&my_arena);
    return 0;
}
