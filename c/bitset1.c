#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Enums
typedef enum {
    RELATION_R1 = 0,
    RELATION_R2 = 1,
    RELATION_R3 = 2,
} Relation;

typedef enum {
    JOIN_INDEX_SCAN,
    JOIN_HASH_JOIN,
    JOIN_NESTED_LOOP,
} JoinType;

// Bit set representation (using uint8_t directly)
typedef uint8_t Relations;

#define BIT_R1 (1U << RELATION_R1)
#define BIT_R2 (1U << RELATION_R2)
#define BIT_R3 (1U << RELATION_R3)

typedef struct {
    double cost;
    JoinType join_type;
} BestPlan;

// Entry wrapper for array-backed lookup map
typedef struct {
    BestPlan plan;
    bool exists;
} MemoEntry;

// Compiler bit intrinsics helpers
#if defined(__GNUC__) || defined(__clang__)
    #define popcount(x) __builtin_popcount(x)
    #define count_trailing_zeros(x) __builtin_ctz(x)
#else
    // Fallback implementations if not using GCC/Clang
    static int popcount(uint8_t x) {
        int count = 0;
        while (x) { count += x & 1; x >>= 1; }
        return count;
    }
    static int count_trailing_zeros(uint8_t x) {
        if (x == 0) return 8;
        int count = 0;
        while ((x & 1) == 0) { count++; x >>= 1; }
        return count;
    }
#endif



int main(void) {
    // 1. Direct-lookup array for 8-bit key space (0 to 255)
    MemoEntry memo[256] = {0};

    Relations set1 = BIT_R1 | BIT_R3;          // {.R1, .R3}
    Relations set2 = BIT_R1 | BIT_R2 | BIT_R3;   // {.R1, .R2, .R3}

    // 2. Native set operations
    Relations union_set = set1 | set2;                 // Bitwise OR
    Relations intersect = set1 & set2;                 // Bitwise AND
    int count           = popcount(set1);              // Built-in popcount (returns 2)
    //set1 & ~set is A - B difference
    // 3. Raw uint8_t masks
    uint8_t raw_mask1 = (uint8_t)set1;                 // Equals 5
    uint8_t raw_mask2 = (uint8_t)set2;                 // Equals 7

    memo[set1] = (MemoEntry){
        .plan = { .cost = 15.4, .join_type = JOIN_INDEX_SCAN },
        .exists = true
    };
    memo[set2] = (MemoEntry){
        .plan = { .cost = 89.1, .join_type = JOIN_HASH_JOIN },
        .exists = true
    };

    // 4. Bit manipulation intrinsic
    int lowest_idx = count_trailing_zeros(raw_mask1);   // Returns 0 (R1)

    if (memo[set1].exists) {
        BestPlan plan = memo[set1].plan;
        const char *type_str = (plan.join_type == JOIN_INDEX_SCAN) ? "Index_Scan" :
                               (plan.join_type == JOIN_HASH_JOIN)  ? "Hash_Join"  : "Nested_Loop";
        printf("Sub-plan cost for {.R1, .R3}: %.1f %s\n", plan.cost, type_str);
    }

    printf("%d\n", union_set);
    printf("%d\n", intersect);
    printf("%d\n", count);

    printf("%d\n", raw_mask1);
    printf("%d\n", raw_mask2);

    return 0;
}
