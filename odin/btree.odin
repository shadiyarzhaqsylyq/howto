package main

import "core:fmt"

// Fan-out parameters.
// For production, choose MAX_KEYS so that Node fits cleanly into 64 or 128 bytes.
MAX_KEYS :: 4 
INVALID_NODE :: max(u32)

Key   :: int
Value :: int

// Dense node representation fitting into a unified flat buffer
Node :: struct {
	count:     u32,
	is_leaf:   bool,
	next_leaf: u32, // Only used when is_leaf == true

	// Internal nodes use keys + children
	// Leaf nodes use keys + values
	keys:      [MAX_KEYS + 1]Key,
	children:  [MAX_KEYS + 2]u32,
	values:    [MAX_KEYS + 1]Value,
}

BPlusTree :: struct {
	root:  u32,
	nodes: [dynamic]Node,
}

bptree_init :: proc(tree: ^BPlusTree, initial_capacity: int = 16) {
	tree.nodes = make([dynamic]Node, 0, initial_capacity)
	tree.root = bptree_alloc_node(tree, is_leaf = true)
}

bptree_destroy :: proc(tree: ^BPlusTree) {
	delete(tree.nodes)
}

// Allocates from the contiguous pool (no individual heap allocations)
bptree_alloc_node :: proc(tree: ^BPlusTree, is_leaf: bool) -> u32 {
	idx := u32(len(tree.nodes))
	node := Node{
		count     = 0,
		is_leaf   = is_leaf,
		next_leaf = INVALID_NODE,
	}
	append(&tree.nodes, node)
	return idx
}

// Search for a key
bptree_find :: proc(tree: ^BPlusTree, key: Key) -> (Value, bool) {
	if len(tree.nodes) == 0 do return 0, false

	curr_idx := tree.root
	for !tree.nodes[curr_idx].is_leaf {
		node := &tree.nodes[curr_idx]
		child_idx := node.count

		for i in 0..<node.count {
			if key < node.keys[i] {
				child_idx = i
				break
			}
		}
		curr_idx = node.children[child_idx]
	}

	leaf := &tree.nodes[curr_idx]
	for i in 0..<leaf.count {
		if leaf.keys[i] == key {
			return leaf.values[i], true
		}
	}

	return 0, false
}

// Insert a key-value pair
bptree_insert :: proc(tree: ^BPlusTree, key: Key, val: Value) {
	// Fixed-size stack tracking traversal path (avoids recursion and allocations)
	path: [32]u32
	path_len: int = 0

	curr_idx := tree.root

	// 1. Traverse to leaf
	for !tree.nodes[curr_idx].is_leaf {
		path[path_len] = curr_idx
		path_len += 1

		node := &tree.nodes[curr_idx]
		child_idx := node.count
		for i in 0..<node.count {
			if key < node.keys[i] {
				child_idx = i
				break
			}
		}
		curr_idx = node.children[child_idx]
	}

	// 2. Insert into leaf
	leaf := &tree.nodes[curr_idx]
	insert_pos := leaf.count
	for i in 0..<leaf.count {
		if leaf.keys[i] == key {
			// Update existing key
			leaf.values[i] = val
			return
		}
		if key < leaf.keys[i] {
			insert_pos = i
			break
		}
	}

	for i := leaf.count; i > insert_pos; i -= 1 {
		leaf.keys[i] = leaf.keys[i - 1]
		leaf.values[i] = leaf.values[i - 1]
	}
	leaf.keys[insert_pos] = key
	leaf.values[insert_pos] = val
	leaf.count += 1

	// 3. Handle split propagation if leaf overflows
	if leaf.count <= MAX_KEYS do return

	// Split Leaf Node
	new_leaf_idx := bptree_alloc_node(tree, is_leaf = true)
	// Re-fetch pointer as appending might reallocate the dynamic array
	leaf = &tree.nodes[curr_idx]
	new_leaf := &tree.nodes[new_leaf_idx]

	mid := leaf.count / 2
	new_leaf_count: u32 = 0

	for i := mid; i < leaf.count; i += 1 {
		new_leaf.keys[new_leaf_count] = leaf.keys[i]
		new_leaf.values[new_leaf_count] = leaf.values[i]
		new_leaf_count += 1
	}

	new_leaf.count = new_leaf_count
	leaf.count = mid

	new_leaf.next_leaf = leaf.next_leaf
	leaf.next_leaf = new_leaf_idx

	// B+ Tree invariant: copy the first key of the new leaf up
	promoted_key := new_leaf.keys[0]
	promoted_child := new_leaf_idx

	// 4. Propagate up internal nodes
	for path_len > 0 {
		path_len -= 1
		parent_idx := path[path_len]
		parent := &tree.nodes[parent_idx]

		p_pos := parent.count
		for i in 0..<parent.count {
			if promoted_key < parent.keys[i] {
				p_pos = i
				break
			}
		}

		for i := parent.count; i > p_pos; i -= 1 {
			parent.keys[i] = parent.keys[i - 1]
			parent.children[i + 1] = parent.children[i]
		}
		parent.keys[p_pos] = promoted_key
		parent.children[p_pos + 1] = promoted_child
		parent.count += 1

		if parent.count <= MAX_KEYS {
			return
		}

		// Split Internal Node
		new_internal_idx := bptree_alloc_node(tree, is_leaf = false)
		parent = &tree.nodes[parent_idx]
		new_internal := &tree.nodes[new_internal_idx]

		p_mid := parent.count / 2
		up_key := parent.keys[p_mid] // Key pulled up to higher level

		new_count: u32 = 0
		new_internal.children[0] = parent.children[p_mid + 1]

		for i := p_mid + 1; i < parent.count; i += 1 {
			new_internal.keys[new_count] = parent.keys[i]
			new_internal.children[new_count + 1] = parent.children[i + 1]
			new_count += 1
		}

		new_internal.count = new_count
		parent.count = p_mid

		promoted_key = up_key
		promoted_child = new_internal_idx
	}

	// 5. New root needed
	new_root_idx := bptree_alloc_node(tree, is_leaf = false)
	new_root := &tree.nodes[new_root_idx]
	new_root.keys[0] = promoted_key
	new_root.children[0] = tree.root
	new_root.children[1] = promoted_child
	new_root.count = 1
	tree.root = new_root_idx
}

// Demonstrates DOD streaming: sequential leaf scan through contiguous indices
bptree_print_all :: proc(tree: ^BPlusTree) {
	if len(tree.nodes) == 0 do return

	// Find first leaf
	curr := tree.root
	for !tree.nodes[curr].is_leaf {
		curr = tree.nodes[curr].children[0]
	}

	fmt.print("Linear scan: ")
	for curr != INVALID_NODE {
		leaf := &tree.nodes[curr]
		for i in 0..<leaf.count {
			fmt.printf("[%d: %d] ", leaf.keys[i], leaf.values[i])
		}
		curr = leaf.next_leaf
	}
	fmt.println()
}

main :: proc() {
	tree: BPlusTree
	bptree_init(&tree, initial_capacity = 32)
	defer bptree_destroy(&tree)

	keys := [?]int{15, 8, 20, 5, 12, 18, 25, 3, 7, 10, 14, 16, 19, 22, 30}

	fmt.println("--- Inserting Keys ---")
	for k in keys {
		bptree_insert(&tree, k, k * 10)
	}

	bptree_print_all(&tree)

	fmt.println("\n--- Lookups ---")
	test_lookups := [?]int{7, 18, 30, 999}
	for k in test_lookups {
		val, found := bptree_find(&tree, k)
		if found {
			fmt.printf("Found %d -> %d\n", k, val)
		} else {
			fmt.printf("Key %d not found\n", k)
		}
	}

	fmt.printf("\nTotal internal node capacity: %d nodes allocated in flat buffer.\n", len(tree.nodes))
}
