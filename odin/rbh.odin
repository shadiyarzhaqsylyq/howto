package main

import "core:fmt"
import "core:math/bits"
import "core:math"
//Robin Hood Hash Map 
Relation :: enum u8 {
    R1, R2, R3, R4, R5, R6, R7, R8,
}

// 8 relations fit into a u8 bit_set
Relations :: bit_set[Relation; u8]

Join_Type :: enum u8 {
    Index_Scan,
    Hash_Join,
    Nested_Loop_Join,
}

Best_Plan :: struct {
    cost:      f64,
    left:      Relations,  // Left sub-tree relation subset
    right:     Relations,  // Right sub-tree relation subset
    join_type: Join_Type,
}

// Helper to look up or initialize a sub-plan
get_best_plan :: proc(memo: map[Relations]Best_Plan, S: Relations) -> (Best_Plan, bool) {
    plan, found := memo[S]
    return plan, found
}

// Helper to update DP memo table if a cheaper join order is found
update_best_plan :: proc(memo: ^map[Relations]Best_Plan, S: Relations, new_plan: Best_Plan) {
    if existing, found := memo[S]; !found || new_plan.cost < existing.cost {
        memo[S] = new_plan
    }
}

main :: proc() {
    memo := make(map[Relations]Best_Plan)
    defer delete(memo)

    // Base tables
    r1: Relations = {.R1}
    r2: Relations = {.R2}
    r3: Relations = {.R3}

    // Set base relation scan costs
    update_best_plan(&memo, r1, Best_Plan{cost = 10.0, join_type = .Index_Scan})
    update_best_plan(&memo, r2, Best_Plan{cost = 25.0, join_type = .Index_Scan})
    update_best_plan(&memo, r3, Best_Plan{cost = 5.0,  join_type = .Index_Scan})

    // DP Step: Try joining {R1} and {R2} -> {R1, R2}
    sub_left  := r1
    sub_right := r2
    joined_set := sub_left + sub_right // Native set union (OR)

    left_plan, _  := get_best_plan(memo, sub_left)
    right_plan, _ := get_best_plan(memo, sub_right)

    // Example cost model calculation
    join_cost := left_plan.cost + right_plan.cost + 15.0 // Join overhead
    
    candidate := Best_Plan{
        cost      = join_cost,
        left      = sub_left,
        right     = sub_right,
        join_type = .Hash_Join,
    }

    update_best_plan(&memo, joined_set, candidate)

    // Print resulting sub-plan
    if plan, ok := get_best_plan(memo, joined_set); ok {
        fmt.printfln("Sub-plan for set %v:", joined_set)
        fmt.printfln("  Cost:      %.2f", plan.cost)
        fmt.printfln("  Left Tree: %v", plan.left)
        fmt.printfln("  Right Tree:%v", plan.right)
        fmt.printfln("  Join Type: %v", plan.join_type)
        fmt.printfln("  Table Count (card): %d", card(joined_set))
    }
}
