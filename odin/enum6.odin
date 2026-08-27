package main

import "core:fmt"

Relation :: enum u8 {
	R1,  R2,  R3,  R4,  R5,  R6,  R7,  R8,
	R9,  R10, R11, R12, R13, R14, R15, R16,
}

Relations :: bit_set[Relation; u64]

BestPlan :: struct {
	cost:  f64,
	valid: bool,
}

N :: len(Relation)        // 16
NUM_STATES :: 1 << N      // 65,536 states

main :: proc() {
	// At N = 16, allocate on the heap to avoid stack overflow risks (~1 MB)
	memo := make([]BestPlan, NUM_STATES)
	defer delete(memo)

	// Create a test bitset with lower and higher relations
	set1: Relations = {.R1, .R5, .R12, .R16}
	raw_mask := transmute(u64)set1

	// O(1) Direct Array Indexing: Store plan
	memo[raw_mask] = BestPlan{
		cost  = 142.50,
		valid = true,
	}

	// O(1) Direct Array Indexing: Lookup plan
	retrieved := memo[raw_mask]

	fmt.println("Set:            ", set1)
	fmt.println("Bitmask Index:  ", raw_mask) // 1 + 16 + 2048 + 32768 = 34833
	fmt.println("Retrieved Cost: ", retrieved.cost)
}
