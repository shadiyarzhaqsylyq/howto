package main

import "core:fmt"
import "core:math/bits"
//Array Indexing <= 16
// Define relations as an Enum
Relation :: enum u8 {
    R1, R2, R3, R4, R5,
}

// Create a native bit_set backed by a 64-bit integer
Relations :: bit_set[Relation; u64]

main :: proc() {
    // 1. Create set {R1, R3, R4} using clean literal syntax
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

    fmt.println("Set:", set1)                          // Output: {.R1, .R3, .R4}
    fmt.println("Raw u64:", raw_mask)                  // Output: 13
    fmt.println("Lowest Relation Index:", lowest_idx)  // Output: 0
    fmt.println("Element Count:", count)              // Output: 3
}


/*
Relation,Bit position,Binary value,Decimal value
R1,bit 0,0b00000001,1
R2,bit 1,0b00000010,2
R3,bit 2,0b00000100,4
R4,bit 3,0b00001000,8
R5,bit 4,0b00010000,16
Combining R1,R3,R4 = 13 = 00001101


*/
