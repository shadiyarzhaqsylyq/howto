#include <stdio.h>
#include <stdint.h>

static uint64_t x = 1;; /* can be seeded with any value. */

uint64_t splitmix64(uint64_t x) {
	uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
	return z ^ (z >> 31);
}


uint64_t get_bucket_index(uint64_t hash, uint64_t capacity) {
    unsigned __int128 product = (unsigned __int128)hash * (unsigned __int128)capacity;
    return (uint64_t)(product >> 64);
}


// Requirements: capacity MUST be a power of two (e.g., 2, 4, 8, 16, 32...)
// Butch N is in memory
// Butch N [bucket 0, bucket 1, bucket 2,...]
// Bucket N+1 [bucket 0, bucket 1, bucket 2,...]
uint64_t get_bucket_index_pow2(uint64_t hash, uint64_t capacity) {
    return hash & (capacity - 1);
}

uint64_t hash_combine(uint64_t h1, uint64_t h2){

return splitmix64(h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2))));

}




// Equivalent to the Odin 'Wide_Composite_Key' struct
typedef struct {
    uint64_t tenant_id;
    uint64_t account_id;
    uint64_t region_id;
    uint8_t  status_flag;
} Wide_Composite_Key;

// Equivalent to 'hash_wide_composite_fast'
uint64_t hash_wide_composite_fast(const Wide_Composite_Key *key, uint64_t seed) {
    // Initialize with seed and first block
    uint64_t h = seed + key->tenant_id + 0x9e3779b97f4a7c15ULL;
    
    // Mix in subsequent fields step-by-step
    h = (h ^ key->account_id) * 0xbf58476d1ce4e5b9ULL;
    h = (h ^ key->region_id)  * 0x94d049bb133111ebULL;
    h = (h ^ (uint64_t)key->status_flag) * 0x9e3779b97f4a7c15ULL;
    
    // Final avalanche step
    h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    return h ^ (h >> 31);
}

int main(void) {
    // Instantiate and initialize the struct fields
    Wide_Composite_Key wide_key = {
        .tenant_id   = 1,
        .account_id  = 5555,
        .region_id   = 99,
        .status_flag = 1
    };

    // Call the function with seed = 0 (default arguments must be explicitly passed in C)
    uint64_t h1 = hash_wide_composite_fast(&wide_key, 0);
    
    // Output matches the 16-character padded uppercase hexadecimal format
    printf("[Technique 3 - Wide >=24B] Mix Hash: 0x%016llX\n", (unsigned long long)h1);

    return 0;
}
