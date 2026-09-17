#include <stdio.h>
#include <string.h>
#define XXH_INLINE_ALL // Inlines xxHash for maximum performance
#include "xxhash.h"    // Requires the official xxHash library header

int main(void) {
    // In C, a string literal is already a contiguous array of bytes (char/uint8_t)
    const char* key = "customer_id_98741";
    size_t len = strlen(key);

    // 1. Default (unseeded) XXH3 64-bit
    // The standard default seed in the xxHash library is 0
    XXH64_hash_t h1 = XXH3_64bits(key, len);
    printf("XXH3_64:        0x%016llx\n", (unsigned long long)h1);

    // 2. Seeded XXH3 64-bit
    uint64_t seed = 0xdeadbeefcafebabeULL;
    XXH64_hash_t h2 = XXH3_64bits_withSeed(key, len, seed);
    printf("XXH3_64 Seeded: 0x%016llx\n", (unsigned long long)h2);

    // 3. XXH3 128-bit
    XXH128_hash_t h128 = XXH3_128bits(key, len);
    // xxHash 128-bit outputs a struct containing two 64-bit integers (.low64 and .high64)
    printf("XXH3_128:       0x%016llx%016llx\n", 
           (unsigned long long)h128.high64, 
           (unsigned long long)h128.low64);

    return 0;
}
