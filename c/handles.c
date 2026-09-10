#include <stdio.h>
#include <stdint.h>

// Define our single integer handle type
typedef uint64_t Handle;

// Define how many bits each piece gets
#define INDEX_BITS 32
#define GENERATION_BITS 32

// Create bitmasks to safely extract the data
#define INDEX_MASK ((1ULL << INDEX_BITS) - 1)          // Lower 32 bits: 0x00000000FFFFFFFF
#define GENERATION_MASK ((1ULL << GENERATION_BITS) - 1) // Used for clean masking

// Pack an index and a generation into a single 64-bit integer
Handle pack_handle(uint32_t index, uint32_t generation) {
    // Shift the generation up by 32 bits, then combine it with the index using bitwise OR
    return ((uint64_t)generation << INDEX_BITS) | (index & INDEX_MASK);
}

// Extract the 32-bit index from the handle
uint32_t get_handle_index(Handle handle) {
    return (uint32_t)(handle & INDEX_MASK);
}

// Extract the 32-bit generation from the handle
uint32_t get_handle_generation(Handle handle) {
    // Shift the bits back down to get the original generation number
    return (uint32_t)(handle >> INDEX_BITS);
}

int main() {
    // Example data
    uint32_t target_index = 1052;
    uint32_t target_generation = 42;

    // 1. Pack them together
    Handle my_handle = pack_handle(target_index, target_generation);
    printf("Packed Handle Value (Hex): 0x%016llX\n", (unsigned long long)my_handle);
    // Visual output will look like: 0x0000002A0000041C
    // Notice how '2A' (42) is on the left, and '41C' (1052) is on the right!

    // 2. Unpack them later in your lookup functions
    uint32_t unpacked_index = get_handle_index(my_handle);
    uint32_t unpacked_generation = get_handle_generation(my_handle);

    printf("Unpacked Index: %u\n", unpacked_index);
    printf("Unpacked Generation: %u\n", unpacked_generation);

    return 0;
}
