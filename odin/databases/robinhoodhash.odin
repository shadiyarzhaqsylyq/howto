package main

import "core:fmt"
import "core:math/bits"

//Robin Hood Hash Map 12 <= N <= 30 tables, 10_000 to 100_000 entries
Relation :: enum u8 { R1, R2, R3 }
Relations :: bit_set[Relation; u8]

Join_Type :: enum u8 {
    Index_Scan,
    Hash_Join,
    Nested_Loop,
}

Best_Plan :: struct {
    cost:      f64,
    join_type: Join_Type,
}

main :: proc() {
    // 1. Idiomatic approach: Map directly on the bit_set type
    memo := make(map[Relations]Best_Plan)
    defer delete(memo)

    set1: Relations = {.R1, .R3}
    set2: Relations = {.R1, .R2, .R3}
	
	// 2. Native set operations (compiles to bitwise OR, AND, etc.)
    union_set := set1 + set2       // Bitwise OR
    intersect := set1 & set2       // Bitwise AND
    count     := card(set1)        // Built-in popcount! Returns 2

    // 3. Convert bitset to raw u64 mask
    raw_mask1 := transmute(u8)set1  // Equals 5
	raw_mask2 := transmute(u8)set2  // Equals 7
	
	
    memo[set1] = Best_Plan{cost = 15.4, join_type = .Index_Scan}
    memo[set2] = Best_Plan{cost = 89.1, join_type = .Hash_Join}
	
    // 4. Bit manipulation intrinsic
    lowest_idx := bits.count_trailing_zeros(raw_mask1) // Returns 0 (R1)

    if plan, ok := memo[set1]; ok {
        fmt.println("Sub-plan cost for {.R1, .R3}:", plan.cost, plan.join_type)
    }

	fmt.printfln("%v", union_set)
	fmt.printfln("%v", intersect)

	fmt.printfln("%d", transmute(u8)union_set)
	fmt.printfln("%d", transmute(u8)intersect)
	fmt.printfln("%d", count)

	fmt.printfln("%d", raw_mask1)
	fmt.printfln("%d", raw_mask2)
	
}
/*




*/
