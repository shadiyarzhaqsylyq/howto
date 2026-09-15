package main

import "core:fmt"
import "core:math"
import "core:slice"
//Direct Array Indexing with context.temp_allocator
Relation :: enum u8 { R1, R2, R3, R4, R5 }
Relations :: bit_set[Relation; u64]

Join_Type :: enum u8 { Scan, Hash_Join, Nested_Loop_Join }

Best_Plan :: struct {
    cost:      f64,
    left:      Relations,
    right:     Relations,
    join_type: Join_Type,
}

Memo_Table :: struct {
    plans: []Best_Plan,
}

// Pass allocator as a parameter (defaults to context.temp_allocator)
init_memo_table :: proc(relation_count: int, allocator := context.temp_allocator) -> Memo_Table {
    size := 1 << uint(relation_count)
    
    // O(1) Arena Bump Allocation — zero malloc overhead
    plans := make([]Best_Plan, size, allocator)

    // Fast batch initialization
    default_plan := Best_Plan{cost = math.INF_F64}
    slice.fill(plans, default_plan)

    return Memo_Table{plans = plans}
}


get_plan :: #force_inline proc(table: ^Memo_Table, set: Relations) -> ^Best_Plan {
    mask := transmute(u64)set
    return &table.plans[mask]
}

main :: proc() {
    // Reset temp arena after query optimization finishes
    defer free_all(context.temp_allocator)

    relation_count := len(Relation)
    memo := init_memo_table(relation_count) // Allocates instantly on the temp arena

    s1: Relations = {.R1, .R3}
    s2: Relations = {.R2, .R4}
    combined := s1 + s2

    p1 := get_plan(&memo, s1)
    p1.cost = 30.0
    p1.join_type = .Scan

    p2 := get_plan(&memo, s2)
    p2.cost = 45.0
    p2.join_type = .Scan

    p_combined := get_plan(&memo, combined)
    candidate_cost := p1.cost + p2.cost + 12.5

    if candidate_cost < p_combined.cost {
        p_combined.cost = candidate_cost
        p_combined.left = s1
        p_combined.right = s2
        p_combined.join_type = .Hash_Join
    }

    fmt.printfln("Optimal Cost: %.2f", p_combined.cost)
}
