#include <stdio.h>
#include <stdint.h>
#include <time.h>

// 1. Defining a power of two size (1024 slots)
#define BUCKETS_POWER_OF_TWO 1024

// 2. Defining a non-power of two size (1000 slots)
#define BUCKETS_NON_POWER_OF_TWO 1000

// Simple 32-bit hash function simulation (Fowler–Noll–Vo style snippet)
uint32_t hash_key(uint32_t id) {
    id = ((id >> 16) ^ id) * 0x45d9f3b;
    id = ((id >> 16) ^ id) * 0x45d9f3b;
    id = (id >> 16) ^ id;
    return id;
}

int main() {
    uint32_t customer_id = 94823;
    uint32_t hash_value = hash_key(customer_id);

    printf("Customer ID: %u\n", customer_id);
    printf("Calculated Hash: 0x%08X (%u)\n\n", hash_value, hash_value);

    // --- APPROACH A: Non-Power of Two (Slow) ---
    // Requires the CPU to perform hardware division, which takes many clock cycles.
    uint32_t slot_slow = hash_value % BUCKETS_NON_POWER_OF_TWO;
    printf("[SLOW] Modulo Slot Lookup (Size %d):\n", BUCKETS_NON_POWER_OF_TWO);
    printf("Formula: %u %% %d\n", hash_value, BUCKETS_NON_POWER_OF_TWO);
    printf("Assigned Bucket Index: %u\n\n", slot_slow);

    // --- APPROACH B: Power of Two (Blazing Fast) ---
    // Because 1024 is a power of two, (1024 - 1) creates a binary mask of 0x3FF (all 1s).
    // The bitwise AND operation isolates the lower bits instantly in 1 CPU cycle.
    uint32_t mask = BUCKETS_POWER_OF_TWO - 1;
    uint32_t slot_fast = hash_value & mask;

    printf("[FAST] Bitwise Masking Slot Lookup (Size %d):\n", BUCKETS_POWER_OF_TWO);
    printf("Mask (Size - 1) in Hex: 0x%X (Binary: 001111111111)\n", mask);
    printf("Formula: 0x%08X & 0x%X\n", hash_value, mask);
    printf("Assigned Bucket Index: %u\n", slot_fast);

    return 0;
}
