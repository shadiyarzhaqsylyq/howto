#include <stdint.h>

// 1. Hasher State
typedef struct {
    uint64_t h;
} HashAccumulator;

// 2. Initialize
static inline HashAccumulator hash_init(uint64_t seed) {
    HashAccumulator acc = { .h = seed ^ 0x9E3779B97F4A7C15ULL };
    return acc;
}

// 3. Add 64-bit integer
static inline void hash_add_u64(HashAccumulator *acc, uint64_t val) {
    acc->h = (acc->h ^ val) * 0xBF58476D1CE4E5B9ULL;
}

// 4. Add small integer (8-bit or 32-bit)
static inline void hash_add_u8(HashAccumulator *acc, uint8_t val) {
    acc->h = (acc->h ^ (uint64_t)val) * 0x94D049BB133111EBULL;
}

// 5. Final Avalanche (run once at the end)
static inline uint64_t hash_finish(HashAccumulator *acc) {
    uint64_t h = acc->h;
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 27;
    h *= 0x94D049BB133111EBULL;
    h ^= h >> 31;
    return h;
}

// For 4 fields:
uint64_t hash_key_4(uint64_t c1, uint64_t c2, uint64_t c3, uint8_t c4, uint64_t seed) {
    HashAccumulator acc = hash_init(seed);
    hash_add_u64(&acc, c1);
    hash_add_u64(&acc, c2);
    hash_add_u64(&acc, c3);
    hash_add_u8(&acc, c4);
    return hash_finish(&acc);
}

// For 6 fields:
uint64_t hash_key_6(uint64_t c1, uint64_t c2, uint64_t c3, uint64_t c4, uint64_t c5, uint64_t c6, uint64_t seed) {
    HashAccumulator acc = hash_init(seed);
    hash_add_u64(&acc, c1);
    hash_add_u64(&acc, c2);
    hash_add_u64(&acc, c3);
    hash_add_u64(&acc, c4);
    hash_add_u64(&acc, c5);
    hash_add_u64(&acc, c6);
    return hash_finish(&acc);
}


static inline uint64_t hash_u64_array(const uint64_t *keys, size_t count, uint64_t seed) {
    uint64_t h = seed ^ 0x9E3779B97F4A7C15ULL;

    // Primes to alternate multipliers and break symmetry
    static const uint64_t primes[2] = {
        0xBF58476D1CE4E5B9ULL,
        0x94D049BB133111EBULL
    };

    for (size_t i = 0; i < count; ++i) {
        h = (h ^ keys[i]) * primes[i & 1];
    }

    // Final SplitMix64 avalanche
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 27;
    h *= 0x94D049BB133111EBULL;
    h ^= h >> 31;

    return h;
}

// 1. Mixed Key (tenant_id + string)
// Pass len if available; otherwise use strlen(dept)
static inline uint64_t hash_mixed_with_string1(uint64_t tenant_id, const char* dept, size_t len, uint64_t seed) {
    uint64_t h_str = XXH3_64bits_withSeed(dept, len, seed);
    
    // Do NOT call splitmix64(tenant_id) here. 
    // hash_combine already runs splitmix64 at the end!
    return hash_combine(tenant_id, h_str);
}

// 2. Mixed Key (order_id + user_id + string)
static inline uint64_t hash_mixed_with_string2(const Short_Composite_Key* key, const char* dept, size_t len, uint64_t seed) {
    uint64_t h_str   = XXH3_64bits_withSeed(dept, len, seed);
    uint64_t h_fixed = hash_combine(key->order_id, key->user_id);
    return hash_combine(h_fixed, h_str);
}

typedef enum {
    COL_TYPE_INT64,
    COL_TYPE_STRING,
    COL_TYPE_INT32
} ColumnType;

typedef struct {
    ColumnType type;
    size_t     offset_in_row; // where the field is inside the row memory
} JoinColumnMeta;

// Dynamically hash any composite key row at runtime
uint64_t hash_dynamic_row(const char* row_data, 
                          const JoinColumnMeta* cols, 
                          size_t num_cols, 
                          uint64_t seed) 
{
    HashAccumulator acc = hash_init(seed);

    for (size_t i = 0; i < num_cols; ++i) {
        const char* field_ptr = row_data + cols[i].offset_in_row;

        switch (cols[i].type) {
            case COL_TYPE_INT64: {
                uint64_t val = *(const uint64_t*)field_ptr;
                hash_add_u64(&acc, val);
                break;
            }
            case COL_TYPE_STRING: {
                // Strings in engines have pointer + length (StringView)
                const StringRef* str = (const StringRef*)field_ptr;
                uint64_t h_str = XXH3_64bits_withSeed(str->data, str->len, seed);
                hash_add_u64(&acc, h_str);
                break;
            }
            case COL_TYPE_INT32: {
                uint32_t val = *(const uint32_t*)field_ptr;
                hash_add_u8(&acc, (uint8_t)val); // or hash_add_u32
                break;
            }
        }
    }

    return hash_finish(&acc);
}

