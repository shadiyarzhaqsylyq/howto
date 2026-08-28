package main

import "core:fmt"
import "core:math"

// Relation sequence index (0 to N-1)
Relation_Seq_Idx :: int

Best_Plan :: struct {
    cost:      f64,
    split_idx: int, // Index 'k' where (start..k) joins with (k+1..end)
}

// O(N^2) Memo Table for LinDP++
LinDP_Memo :: struct {
    plans: []Best_Plan,
    N:     int,
}

init_lindp_memo :: proc(N: int) -> LinDP_Memo {
    plans := make([]Best_Plan, N * N)
    for i in 0..<(N * N) {
        plans[i].cost = math.INF_F64
    }
    return LinDP_Memo{plans = plans, N = N}
}

free_lindp_memo :: proc(memo: ^LinDP_Memo) {
    delete(memo.plans)
}

// O(1) direct lookup using sequence indices (start, end)
get_plan :: proc(memo: ^LinDP_Memo, start, end: int) -> ^Best_Plan {
    return &memo.plans[start * memo.N + end]
}

main :: proc() {
    N := 100 // 100 tables query!
    memo := init_lindp_memo(N)
    defer free_lindp_memo(&memo)

    // Example: Lookup best plan for relations from index 5 to index 20
    plan := get_plan(&memo, 5, 20)
    plan.cost = 142.50
    plan.split_idx = 12

    retrieved := get_plan(&memo, 5, 20)
    fmt.printfln("LinDP++ Memo Table size for %d tables: %d entries", N, len(memo.plans))
    fmt.printfln("Sub-sequence [5..20] Cost: %.2f (Split at %d)", retrieved.cost, retrieved.split_idx)
}
