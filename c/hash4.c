#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define XXH_INLINE_ALL 
#include "xxhash.h"

// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------

typedef struct {
    uint64_t order_id;
    uint64_t user_id;
} Short_Composite_Key;




// Daniel Lemire (Fastrange)
static inline uint64_t get_bucket_index(uint64_t hash, uint64_t capacity) {
    unsigned __int128 product = (unsigned __int128)hash * (unsigned __int128)capacity;
    return (uint64_t)(product >> 64);
}


uint64_t get_bucket_index_pow2(uint64_t hash, uint64_t capacity) {
    return hash & (capacity - 1);
}

// SplitMix64 PRNG / Finalizer
// JOIN ON a.id = b.id
static inline uint64_t splitmix64(uint64_t x) {
    uint64_t z = x + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline uint64_t hash_combine(uint64_t h1, uint64_t h2) {
    return splitmix64(h1 ^ (h2 + 0x9E3779B97F4A7C15ULL + (h1 << 6) + (h1 >> 2)));
}

// -----------------------------------------------------------------------------
// Specialized Hash Functions
// -----------------------------------------------------------------------------

// 1. Mixed Key (2 columns: tenant_id + string)
// JOIN ON a.tenant_id = b.tenant_id AND a.dept = b.dept
uint64_t hash_mixed_with_string1(uint64_t tenant_id, const char* dept, uint64_t seed) {
    uint64_t h_str = XXH64(dept, strlen(dept), seed);
    uint64_t h_int = splitmix64(tenant_id);
    return hash_combine(h_int, h_str);
}

// Mixed Key (3 columns: order_id + user_id + string)
// JOIN ON a.order_id = b.order_id AND a.user_id = b.user_id AND a.dept = b.dept
uint64_t hash_mixed_with_string2(const Short_Composite_Key* key, const char* dept, uint64_t seed) {
    uint64_t h_str   = XXH64(dept, strlen(dept), seed);
    uint64_t h_fixed = hash_combine(key->order_id, key->user_id);
    return hash_combine(h_fixed, h_str);
}

// 2. Short Fixed Key (<= 16B)
// JOIN ON a.order_id = b.order_id AND a.tenant_id = b.tenant_id
uint64_t hash_short_composite(const Short_Composite_Key* key) {
    uint64_t h1 = splitmix64(key->order_id);
    uint64_t h2 = splitmix64(key->user_id);
    return hash_combine(h1, h2);
}

static inline uint64_t hash_short_composite_optimized(const Short_Composite_Key* key) {
    return hash_combine(key->order_id, key->user_id);
}




typedef struct {
    uint64_t tenant_id;// 8 bytes
    uint64_t account_id;// 8 bytes
    uint64_t region_id; // 8 bytes
    uint8_t  status_flag; // 1 byte
} Wide_Composite_Key; // 32 bytes, no 25 bytes because of padding

//Best for without padding
static inline uint64_t key_hash(const Wide_Composite_Key* key, uint64_t seed) {
    uint64_t h = seed ^ 0x9E3779B97F4A7C15ULL;

    h = (h ^ key->tenant_id)             * 0xBF58476D1CE4E5B9ULL;
    h = (h ^ key->account_id)            * 0x94D049BB133111EBULL;
    h = (h ^ key->region_id)             * 0x9E3779B97F4A7C15ULL;
    h = (h ^ (uint64_t)key->status_flag) * 0xBF58476D1CE4E5B9ULL;

    // Final avalanche mixer
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;

    return h;
}

// 3. Wide Fixed Key (>= 24B)
uint64_t hash_wide_composite(const Wide_Composite_Key *key, uint64_t seed) {
    return XXH64(key, sizeof(Wide_Composite_Key), seed);
}

// Passing by value lets the compiler optimize field layout in registers
static inline uint64_t hash_wide_composite_optimized(const Wide_Composite_Key *key, uint64_t seed) {
    // 1. Initialize with seed
    uint64_t h = seed ^ 0x9E3779B97F4A7C15ULL;

    // 2. Mix EVERY field with its own prime multiply (prevents ID cancellation)
    h = (h ^ key->tenant_id)             * 0xBF58476D1CE4E5B9ULL;
    h = (h ^ key->account_id)            * 0x94D049BB133111EBULL;
    h = (h ^ key->region_id)             * 0x9E3779B97F4A7C15ULL;
    h = (h ^ (uint64_t)key->status_flag) * 0xBF58476D1CE4E5B9ULL;

    // 3. Full SplitMix64 / Stafford-13 finalizer (guarantees perfect 50% avalanche)
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 27;
    h *= 0x94D049BB133111EBULL;
    h ^= h >> 31;

    return h;
}




// Entry Point


int main(void) {
    Wide_Composite_Key wide_key = {
        .tenant_id   = 1,
        .account_id  = 5555,
        .region_id   = 99,
        .status_flag = 1
    };
    
    uint64_t h1 = hash_wide_composite_optimized(&wide_key, 0);
    printf("[Technique 3 - Wide >=24B]  Hash: 0x%016llX\n", (unsigned long long)h1);

    const uint64_t CAPACITY = 1024;

    // 1. Single int join
    uint64_t h2 = splitmix64(100921);
    uint64_t bucket1 = get_bucket_index_pow2(h2, CAPACITY);

    // 2. Composite fixed join
    Short_Composite_Key short_key = { .order_id = 1001, .user_id = 9999 };
    uint64_t h3 = hash_short_composite(&short_key);
    uint64_t bucket2 = get_bucket_index_pow2(h3, CAPACITY);

    // 3. Mixed key join (fixed + string)
    uint64_t h4 = hash_mixed_with_string1(1002, "engineering", 0);
    uint64_t bucket3 = get_bucket_index_pow2(h4, CAPACITY);

    printf("Single Int Bucket:      %llu\n", (unsigned long long)bucket1);
    printf("Composite Fixed Bucket: %llu\n", (unsigned long long)bucket2);
    printf("Mixed String Bucket:    %llu\n", (unsigned long long)bucket3);



    Short_Composite_Key key1 = { .order_id = 1001, .user_id = 9999 };
    const char* dept1 = "engineering";

    Short_Composite_Key key2 = { .order_id = 1001, .user_id = 9999 };
    const char* dept2 = "engineering";

    Short_Composite_Key key3 = { .order_id = 1001, .user_id = 9999 };
    const char* dept3 = "marketing";

    uint64_t h5 = hash_mixed_with_string2(&key1, dept1, 0);
    uint64_t h6 = hash_mixed_with_string2(&key2, dept2, 0);
    uint64_t h7 = hash_mixed_with_string2(&key3, dept3, 0);

    printf("Row 1 Hash: 0x%016llx\n", (unsigned long long)h5);
    printf("Row 2 Hash: 0x%016llx (Matches Row 1: %s)\n", (unsigned long long)h6, (h5 == h6) ? "true" : "false");
    printf("Row 3 Hash: 0x%016llx (Matches Row 1: %s)\n", (unsigned long long)h7, (h5 == h7) ? "true" : "false");

    return 0;
}
