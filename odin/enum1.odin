package main

import "core:fmt"
//Robin Hood Hash Map N > 16
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

    set_a: Relations = {.R1, .R3}
    set_b: Relations = {.R1, .R2, .R3}

    memo[set_a] = Best_Plan{cost = 15.4, join_type = .Index_Scan}
    memo[set_b] = Best_Plan{cost = 89.1, join_type = .Hash_Join}

    if plan, ok := memo[set_a]; ok {
        fmt.println("Sub-plan cost for {.R1, .R3}:", plan.cost, plan.join_type)
    }
}
/*

    set1: Relations = {.R1, .R3, .R4}
    set2: Relations = {.R3, .R5}

    // 2. Native set operations (compiles to bitwise OR, AND, etc.)
    union_set := set1 + set2       // Bitwise OR
    intersect := set1 & set2       // Bitwise AND
    count     := card(set1)        // Built-in popcount! Returns 3

    // 3. Convert bitset to raw u64 mask
    raw_mask := transmute(u64)set1  // Equals 13 (0b00001101)

    // 4. Bit manipulation intrinsic
    lowest_idx := bits.count_trailing_zeros(raw_mask) // Returns 0 (R1)

Relation,Bit position,Binary value,Decimal value
R1,bit 0,0b00000001,1
R2,bit 1,0b00000010,2
R3,bit 2,0b00000100,4
R4,bit 3,0b00001000,8
R5,bit 4,0b00010000,16
Combining R1,R3,R4 = 13 = 00001101

*/
