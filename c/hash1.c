#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
/*
Use Packed Struct + One Shot when all key columsn are fixed-width(integers, UUIDs, Dates)
Use Packed Struct + Seed Chaining when composite key has both fixed-width columns and variable-length data(like VARCHAR/string)
because they can be different in sizes('finance','IT' and etc.)

mm3finalizer is used when BIGINT/u64
WHERE a.id = b.id

Composite fixed columns(Packed Struct + One-Shot XXH64)
WHERE a.tenant = b.tenant AND a.user_id = b.user_id

Mixed columns with strings(Packed Struct + Seed Chaining)
WHERE a.tenant = b.tenant AND a.name = b.name

Use get_bucket_index_power_of_two() or fastrange() in the end for output values of 
mm3finalizer,Packed Struct + One-Shot,Packed Struct + Seed Chaining

Write-Ahead Log (WAL) Checksums
The Problem: When a database appends records to a WAL file to guarantee durability (ACID), a system crash mid-write can result in a "torn write" (a partially written block of data).

The Role of XXH64: Engines append an XXH64 checksum to each WAL record or log block. During crash recovery, the engine computes the hash of the read records and compares it against the stored checksum. If they don't match, it knows it has hit the end of valid log data or encountered corruption.

Page & Block Level Integrity
Beyond the WAL, data pages stored in data files often contain page headers with checksums. When a page is loaded from disk into the buffer pool, the engine validates the checksum to ensure the storage medium hasn't silently corrupted the bytes.

Hash Indexes, Joins, and Partitioning
Outside of persistence, XXH64 is heavily used in memory for hash tables, hash joins, bloom filters, and sharding/partitioning keys due to its rapid mixing function and excellent avalanche effect.


*/
// Requires xxHash header (https://github.com/Cyan4973/xxHash)
// Compile with: gcc main.c -lxxhash
#define XXH_INLINE_ALL
#include "xxhash.h" //<xxhash.h>

// Universal bucket indexer
static inline uint64_t get_bucket_index_power_of_two(uint64_t hash, uint64_t capacity) {
    return hash & (capacity - 1);
}

// -------------------------------------------------------------
// CASE 1: Single Integer Column (using mm3 finalizer)
// -------------------------------------------------------------
static inline uint64_t hash_single_int(uint64_t id) {
    uint64_t h = id;
    h = (h ^ (h >> 33)) * 0xff51afd7ed558ccdULL;
    h = (h ^ (h >> 33)) * 0xc4ceb9fe1a85ec53ULL;
    return h ^ (h >> 33);
}

// -------------------------------------------------------------
// CASE 2: Composite Fixed Columns (Packed Struct + One-Shot)
// -------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
    uint32_t tenant_id; // 4 bytes
    uint64_t user_id;   // 8 bytes
} Fixed_Key;            // Total: exactly 12 contiguous bytes
#pragma pack(pop)

static inline uint64_t hash_composite_fixed(const Fixed_Key* key, uint64_t seed) {
    return XXH64(key, sizeof(Fixed_Key), seed);
}

// -------------------------------------------------------------
// CASE 3: Mixed Columns (Packed Struct + Seed Chaining)
// -------------------------------------------------------------
static inline uint64_t hash_mixed_with_string(const Fixed_Key* fixed, const char* dept, uint64_t seed) {
    uint64_t h1 = XXH64(fixed, sizeof(Fixed_Key), seed);     // Hash fixed part
    return XXH64(dept, strlen(dept), h1);                     // Seed-chain into string
}

int main(void) {
    const uint64_t CAPACITY = 65536; // Must be power of 2
    const uint64_t query_seed = 0xdeadbeefULL;

    // 1. Single int join
    uint64_t h1 = hash_single_int(100921);
    uint64_t bucket1 = get_bucket_index_power_of_two(h1, CAPACITY);

    // 2. Composite fixed join
    Fixed_Key fixed_k = { .tenant_id = 1, .user_id = 100921 };
    uint64_t h2 = hash_composite_fixed(&fixed_k, query_seed);
    uint64_t bucket2 = get_bucket_index_power_of_two(h2, CAPACITY);

    // 3. Mixed key join (fixed + string)
    uint64_t h3 = hash_mixed_with_string(&fixed_k, "engineering", query_seed);
    uint64_t bucket3 = get_bucket_index_power_of_two(h3, CAPACITY);

    printf("Single Int Bucket:      %" PRIu64 "\n", bucket1);
    printf("Composite Fixed Bucket: %" PRIu64 "\n", bucket2);
    printf("Mixed String Bucket:    %" PRIu64 "\n", bucket3);

    // Row 1
    Fixed_Key key1 = { .tenant_id = 42, .user_id = 100921 };
    const char* dept1 = "engineering";

    // Row 2 from Table B (exact match)
    Fixed_Key key2 = { .tenant_id = 42, .user_id = 100921 };
    const char* dept2 = "engineering";

    // Row 3 from Table C (different department)
    Fixed_Key key3 = { .tenant_id = 42, .user_id = 100921 };
    const char* dept3 = "marketing";

    uint64_t h4 = hash_mixed_with_string(&key1, dept1, query_seed);
    uint64_t h5 = hash_mixed_with_string(&key2, dept2, query_seed);
    uint64_t h6 = hash_mixed_with_string(&key3, dept3, query_seed);

    printf("Row 1 Hash: 0x%016" PRIx64 "\n", h4);
    printf("Row 2 Hash: 0x%016" PRIx64 " (Matches Row 1: %s)\n", h5, (h4 == h5) ? "true" : "false");
    printf("Row 3 Hash: 0x%016" PRIx64 " (Matches Row 1: %s)\n", h6, (h4 == h6) ? "true" : "false");

    return 0;
}
