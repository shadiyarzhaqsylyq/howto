/* System R Sellinger Optimizer

*/

package main

import "core:fmt"
import "core:math"
import "core:math/bits"
import "core:mem"

// Hardware cost constants (relative CPU / I/O weights)
PAGE_IO_COST     :: 1.0     // Cost to read 1 disk page
CPU_TUPLE_COST   :: 0.01    // Cost to evaluate 1 tuple in CPU
HASH_BUILD_COST  :: 0.02    // Cost to insert 1 tuple into hash table
HASH_PROBE_COST  :: 0.015   // Cost to probe hash table
SORT_FACTOR      :: 0.05    // N log N sorting multiplier

NO_ORDER         :: -1
MAX_RELATIONS    :: 32

Col_ID   :: int
Node_Set :: u64

/* --- Schema & Catalog Structures --- */

Index :: struct {
	name:      string,
	col:       Col_ID,
	clustered: bool,
	pages:     f64,
}

Table :: struct {
	id:        int,
	name:      string,
	row_count: f64,
	pages:     f64,
	indexes:   [dynamic]Index,
}

Join_Predicate :: struct {
	t1:          int,
	c1:          Col_ID,
	t2:          int,
	c2:          Col_ID,
	selectivity: f64,
}

Query :: struct {
	tables:             [dynamic]Table,
	joins:              [dynamic]Join_Predicate,
	interesting_orders: [dynamic]Col_ID, // e.g. from ORDER BY / GROUP BY
}

/* --- Plan Representation --- */

Plan_Type :: enum {
	Seq_Scan,
	Index_Scan,
	Nested_Loop_Join,
	Index_Join,
	Hash_Join,
	Sort,
}

Plan :: struct {
	type:        Plan_Type,
	cost:        f64,
	cardinality: f64,
	order:       Col_ID,   // Sorted column, or NO_ORDER
	relations:   Node_Set, // Bitmask of tables in this plan
	left:        ^Plan,    // Left subplan
	right:       ^Plan,    // Base table plan for Left-Deep (nil for scans)
	table_id:    int,      // For scans
	index_name:  string,   // For index scans
}

Optimizer :: struct {
	arena:     mem.Arena,
	allocator: mem.Allocator,
	query:     ^Query,
	dp_table:  map[Node_Set][dynamic]^Plan,
}

/* --- Helpers --- */

make_plan :: proc(opt: ^Optimizer, type: Plan_Type) -> ^Plan {
	p := new(Plan, opt.allocator)
	p.type = type
	p.order = NO_ORDER
	return p
}

is_order_interesting :: proc(q: ^Query, col: Col_ID) -> bool {
	if col == NO_ORDER do return false
	for o in q.interesting_orders {
		if o == col do return true
	}
	return false
}

// Pruning Rule: For subset S, keep the cheapest plan overall AND the
// cheapest plan for each interesting order.
prune_or_add_plan :: proc(opt: ^Optimizer, s: Node_Set, candidate: ^Plan) {
	plans := &opt.dp_table[s]

	for i := 0; i < len(plans); i += 1 {
		existing := plans[i]

		// If same sort property
		if existing.order == candidate.order {
			if candidate.cost < existing.cost {
				plans[i] = candidate // Found a cheaper plan with the same order
			}
			return
		}
	}

	// If it provides a new distinct interesting order, or is the first plan
	append(plans, candidate)
}

find_join_predicate :: proc(q: ^Query, s1: Node_Set, t2_id: int) -> (Join_Predicate, bool) {
	for jp in q.joins {
		t1_in_s1 := (Node_Set(1) << u32(jp.t1) & s1) != 0
		t2_in_s1 := (Node_Set(1) << u32(jp.t2) & s1) != 0

		if (t1_in_s1 && jp.t2 == t2_id) || (t2_in_s1 && jp.t1 == t2_id) {
			return jp, true
		}
	}
	return Join_Predicate{}, false
}

/* --- Phase 1: Single-Relation Access Paths --- */

enumerate_single_relations :: proc(opt: ^Optimizer) {
	for &t in opt.query.tables {
		s := Node_Set(1) << u32(t.id)
		opt.dp_table[s] = make([dynamic]^Plan, opt.allocator)

		// 1. Sequential Table Scan
		seq := make_plan(opt, .Seq_Scan)
		seq.relations = s
		seq.table_id = t.id
		seq.cardinality = t.row_count
		seq.cost = (t.pages * PAGE_IO_COST) + (t.row_count * CPU_TUPLE_COST)
		seq.order = NO_ORDER
		prune_or_add_plan(opt, s, seq)

		// 2. Index Scans
		for idx in t.indexes {
			iscan := make_plan(opt, .Index_Scan)
			iscan.relations = s
			iscan.table_id = t.id
			iscan.index_name = idx.name
			iscan.cardinality = t.row_count

			// Clustered indexes avoid separate heap page random lookups
			io_cost := idx.pages * PAGE_IO_COST
			if !idx.clustered {
				io_cost += t.row_count * PAGE_IO_COST // Unclustered pointer chase
			}

			iscan.cost = io_cost + (t.row_count * CPU_TUPLE_COST)
			iscan.order = idx.col // Physical sort order established by index!

			prune_or_add_plan(opt, s, iscan)
		}
	}
}

/* --- Phase 2: Left-Deep Join Enumeration --- */

enumerate_joins :: proc(opt: ^Optimizer) {
	n := len(opt.query.tables)

	// Level-by-level (size k = 2 ... n)
	for size := 2; size <= n; size += 1 {
		for mask in 1 ..< (u32(1) << u32(n)) {
			if int(bits.count_ones(mask)) != size do continue

			s := Node_Set(mask)
			opt.dp_table[s] = make([dynamic]^Plan, opt.allocator)

			// Left-Deep rule: Plan(k) = Plan(k-1) ⨝ BaseTable(R)
			for t_idx in 0 ..< n {
				t_mask := Node_Set(1) << u32(t_idx)
				if (s & t_mask) == 0 do continue

				s_prime := s & ~t_mask // Subplan of size k - 1
				if !(s_prime in opt.dp_table) do continue

				jp, connected := find_join_predicate(opt.query, s_prime, t_idx)
				if !connected {
					// Selinger skips Cartesian products if connected joins exist
					continue
				}

				base_plans := opt.dp_table[t_mask]
				left_plans := opt.dp_table[s_prime]

				for p_left in left_plans {
					for p_right in base_plans {
						card := p_left.cardinality * p_right.cardinality * jp.selectivity

						// --- Operator A: Hash Join ---
						// Builds hash table on right (base table), probes with stream from left
						hj := make_plan(opt, .Hash_Join)
						hj.relations = s
						hj.cardinality = card
						hj.left = p_left
						hj.right = p_right
						hj.cost = p_left.cost + p_right.cost + 
						          (p_right.cardinality * HASH_BUILD_COST) + 
						          (p_left.cardinality * HASH_PROBE_COST)
						hj.order = NO_ORDER // Hash join destroys order
						prune_or_add_plan(opt, s, hj)

						// --- Operator B: Nested Loop Join ---
						// Pipelined: Outer (left) loop scans inner (right)
						nlj := make_plan(opt, .Nested_Loop_Join)
						nlj.relations = s
						nlj.cardinality = card
						nlj.left = p_left
						nlj.right = p_right
						nlj.cost = p_left.cost + (p_left.cardinality * p_right.cost)
						nlj.order = p_left.order // Preserves outer stream order!
						prune_or_add_plan(opt, s, nlj)

						// --- Operator C: Index Join ---
						// If right child is an index scan matching the join key
						if p_right.type == .Index_Scan && p_right.order == jp.c2 {
							ij := make_plan(opt, .Index_Join)
							ij.relations = s
							ij.cardinality = card
							ij.left = p_left
							ij.right = p_right
							// 3 B-Tree I/O lookups per left row
							ij.cost = p_left.cost + (p_left.cardinality * (3.0 * PAGE_IO_COST + CPU_TUPLE_COST))
							ij.order = p_left.order
							prune_or_add_plan(opt, s, ij)
						}
					}
				}
			}
		}
	}
}

/* --- Phase 3: Final Access Path & ORDER BY Resolution --- */

find_best_plan :: proc(opt: ^Optimizer, required_order: Col_ID) -> ^Plan {
	n := len(opt.query.tables)
	all_nodes := (Node_Set(1) << u32(n)) - 1
	plans := opt.dp_table[all_nodes]

	if len(plans) == 0 do return nil

	cheapest_overall: ^Plan = nil
	cheapest_ordered: ^Plan = nil

	for p in plans {
		if cheapest_overall == nil || p.cost < cheapest_overall.cost {
			cheapest_overall = p
		}
		if p.order == required_order {
			if cheapest_ordered == nil || p.cost < cheapest_ordered.cost {
				cheapest_ordered = p
			}
		}
	}

	if required_order == NO_ORDER do return cheapest_overall

	// Calculate cost if we take the cheapest overall plan and append an explicit Sort
	sort_cost := cheapest_overall.cardinality * math.log2(cheapest_overall.cardinality) * SORT_FACTOR
	total_plan_with_sort_cost := cheapest_overall.cost + sort_cost

	// Did an naturally ordered plan win over doing an explicit sort?
	if cheapest_ordered != nil && cheapest_ordered.cost <= total_plan_with_sort_cost {
		return cheapest_ordered
	}

	// Otherwise, insert an explicit sort node over the cheapest plan
	sort_node := make_plan(opt, .Sort)
	sort_node.relations = all_nodes
	sort_node.cardinality = cheapest_overall.cardinality
	sort_node.cost = total_plan_with_sort_cost
	sort_node.order = required_order
	sort_node.left = cheapest_overall
	return sort_node
}

/* --- Plan Printer --- */

print_plan :: proc(p: ^Plan, tables: []Table, depth := 0) {
	if p == nil do return

	for _ in 0 ..< depth do fmt.print("  ")

	switch p.type {
	case .Seq_Scan:
		fmt.printf("-> SeqScan(%s) [Card: %.0f, Cost: %.2f]\n", 
			tables[p.table_id].name, p.cardinality, p.cost)
	case .Index_Scan:
		fmt.printf("-> IndexScan(%s via %s, Order: Col %d) [Card: %.0f, Cost: %.2f]\n", 
			tables[p.table_id].name, p.index_name, p.order, p.cardinality, p.cost)
	case .Hash_Join:
		fmt.printf("-> HashJoin [Card: %.0f, Cost: %.2f]\n", p.cardinality, p.cost)
		print_plan(p.left, tables, depth + 1)
		print_plan(p.right, tables, depth + 1)
	case .Nested_Loop_Join:
		fmt.printf("-> NestedLoopJoin [Card: %.0f, Cost: %.2f, PreservedOrder: %d]\n", 
			p.cardinality, p.cost, p.order)
		print_plan(p.left, tables, depth + 1)
		print_plan(p.right, tables, depth + 1)
	case .Index_Join:
		fmt.printf("-> IndexJoin [Card: %.0f, Cost: %.2f, PreservedOrder: %d]\n", 
			p.cardinality, p.cost, p.order)
		print_plan(p.left, tables, depth + 1)
		print_plan(p.right, tables, depth + 1)
	case .Sort:
		fmt.printf("-> ExplicitSort(Order: Col %d) [Cost: %.2f]\n", p.order, p.cost)
		print_plan(p.left, tables, depth + 1)
	}
}

/* --- Driver / Verification Example --- */

main :: proc() {
	// 1. Setup Arena Allocator (Zero Memory Leaks)
	arena_buf := make([]u8, 16 * mem.Megabyte)
	defer delete(arena_buf)
	
	arena: mem.Arena
	mem.arena_init(&arena, arena_buf)

	opt := Optimizer{
		arena     = arena,
		allocator = mem.arena_allocator(&arena),
		dp_table  = make(map[Node_Set][dynamic]^Plan),
	}

	// 2. Define Schema
	// Columns:
	// users.id = 0
	// orders.user_id = 0, orders.id = 1
	// lineitem.order_id = 1
	COL_USERS_ID     :: 0
	COL_ORDERS_USERID :: 0
	COL_ORDERS_ID     :: 1
	COL_LINEITEM_OID  :: 1

	users := Table{
		id = 0, name = "users", row_count = 10_000, pages = 200,
		indexes = make([dynamic]Index, opt.allocator),
	}
	append(&users.indexes, Index{name = "idx_users_pk", col = COL_USERS_ID, clustered = true, pages = 30})

	orders := Table{
		id = 1, name = "orders", row_count = 100_000, pages = 2_000,
		indexes = make([dynamic]Index, opt.allocator),
	}
	append(&orders.indexes, Index{name = "idx_orders_uid", col = COL_ORDERS_USERID, clustered = false, pages = 250})

	lineitem := Table{
		id = 2, name = "lineitem", row_count = 500_000, pages = 10_000,
		indexes = make([dynamic]Index, opt.allocator),
	}
	append(&lineitem.indexes, Index{name = "idx_lineitem_oid", col = COL_LINEITEM_OID, clustered = false, pages = 1_200})

	// 3. Define Query: users ⨝ orders ⨝ lineitem WITH ORDER BY users.id
	query := Query{
		tables             = make([dynamic]Table, opt.allocator),
		joins              = make([dynamic]Join_Predicate, opt.allocator),
		interesting_orders = make([dynamic]Col_ID, opt.allocator),
	}
	append(&query.tables, users, orders, lineitem)

	// users.id = orders.user_id
	append(&query.joins, Join_Predicate{t1 = 0, c1 = COL_USERS_ID, t2 = 1, c2 = COL_ORDERS_USERID, selectivity = 0.0001})
	// orders.id = lineitem.order_id
	append(&query.joins, Join_Predicate{t1 = 1, c1 = COL_ORDERS_ID, t2 = 2, c2 = COL_LINEITEM_OID, selectivity = 0.00002})

	// Query requires output sorted by users.id (e.g. ORDER BY users.id)
	append(&query.interesting_orders, COL_USERS_ID)

	opt.query = &query

	// 4. Run System R Optimizer
	fmt.println("Optimizing query with System R (Selinger)...")
	enumerate_single_relations(&opt)
	enumerate_joins(&opt)

	best_plan := find_best_plan(&opt, COL_USERS_ID)

	fmt.println("\nGenerated Optimal Execution Plan:")
	print_plan(best_plan, query.tables[:])
}
