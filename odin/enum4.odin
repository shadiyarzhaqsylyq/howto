package main

import "core:fmt"
//Robin Hood Hash Map N > 12
Relation :: enum u8 { R1, R2, R3, R4, R5 }
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
    set_b: Relations = {.R2, .R4, .R5}

    memo[set_a] = Best_Plan{cost = 15.4, join_type = .Index_Scan}
    memo[set_b] = Best_Plan{cost = 89.1, join_type = .Hash_Join}

    if plan, ok := memo[set_a]; ok {
        fmt.println("Sub-plan cost for {.R1, .R3}:", plan.cost, plan.join_type)
    }
}
