package main

import "core:fmt"
import "core:math"

Relation :: enum u8 {
    R1, R2, R3, R4, R5,
}

// u64 backing for fast bitmask transmute
Relations :: bit_set[Relation; u64]

Join_Type :: enum u8 {
    Scan,
    Hash_Join,
    Nested_Loop_Join,
}

Best_Plan :: struct {
    cost:      f64,
    left:      Relations,
    right:     Relations,
    join_type: Join_Type,
}

// DP Table structure wrapping heap-allocated flat slice
Memo_Table :: struct {
    plans: []Best_Plan,
    count: int,
}

// Heap-allocate memo array to prevent stack overflow
init_memo_table :: proc(relation_count: int) -> Memo_Table {
    size := 1 << uint(relation_count)
    plans := make([]Best_Plan, size)
    
    // Initialize all costs to infinity (so < comparison works during DP)
    for i in 0..<size {
        plans[i].cost = math.INF_F64
    }
    
    return Memo_Table{plans = plans, count = relation_count}
}

free_memo_table :: proc(table: ^Memo_Table) {
    delete(table.plans)
}

// O(1) direct lookup using bitmask transmute
get_plan :: proc(table: ^Memo_Table, set: Relations) -> ^Best_Plan {
    mask := transmute(u64)set
    return &table.plans[mask]
}

main :: proc() {
    relation_count := len(Relation)
    memo := init_memo_table(relation_count)
    defer free_memo_table(&memo)

    // Define relation subsets
    s1: Relations = {.R1, .R3}
    s2: Relations = {.R2, .R4}
    combined := s1 + s2 // {.R1, .R2, .R3, .R4}

    // Direct offset access to s1
    p1 := get_plan(&memo, s1)
    p1.cost = 30.0
    p1.join_type = .Scan

    // Direct offset access to s2
    p2 := get_plan(&memo, s2)
    p2.cost = 45.0
    p2.join_type = .Scan

    // Perform DP state transition
    p_combined := get_plan(&memo, combined)
    candidate_cost := p1.cost + p2.cost + 12.5 // Join cost calculation

    if candidate_cost < p_combined.cost {
        p_combined.cost      = candidate_cost
        p_combined.left      = s1
        p_combined.right     = s2
        p_combined.join_type = .Hash_Join
    }

    // Output results
    retrieved := get_plan(&memo, combined)
    mask_val  := transmute(u64)combined

    fmt.printfln("Combined Bitmask Index: %d (0b%b)", mask_val, mask_val)
    fmt.printfln("Optimal Cost:           %.2f", retrieved.cost)
    fmt.printfln("Left Sub-plan:          %v", retrieved.left)
    fmt.printfln("Right Sub-plan:         %v", retrieved.right)
    fmt.printfln("Join Type:              %v", retrieved.join_type)
}
