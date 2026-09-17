// WAL chunk verification: hashing multi-kilobyte log segments that arrive in streams
// Large BLOB/Text columns: hashing 100MB files piece-by-piece so you dont load the entire
// file into RAM.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#define XXH_INLINE_ALL
#include "xxhash.h"

int main(void) {
    // XXH64 streaming state stack allocation
    XXH64_state_t state;
    uint64_t seed = 0;
    
    // Initialize/Reset state
    XXH64_reset(&state, seed);

    const char* chunk1 = "WAL block 1...";
    const char* chunk2 = "WAL block 2...";

    // Feed data chunks iteratively
    XXH64_update(&state, chunk1, strlen(chunk1));
    XXH64_update(&state, chunk2, strlen(chunk2));

    // Calculate final 64-bit digest
    XXH64_hash_t digest = XXH64_digest(&state);

    printf("Streaming XXH64 Digest: 0x%016" PRIx64 "\n", digest);

    return 0;
}
