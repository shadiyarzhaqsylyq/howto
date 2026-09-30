package main

import "core:fmt"
import "core:strings"

TABLE_SIZE :: 7

// Linked list node for collision chains
Node :: struct {
	key:   string,
	value: int,
	next:  ^Node,
}

// Hash table bucket array
HashTable :: struct {
	buckets: []^Node,
}

// String hash function (djb2 algorithm)
hash :: proc(key: string, table_size: int) -> int {
	hash_val: u64 = 5381
	for b in transmute([]u8)key {
		hash_val = ((hash_val << 5) + hash_val) + u64(b) // hash * 33 + b
	}
	return int(hash_val % u64(table_size))
}

// Initialize a new hash table
create_table :: proc(size: int, allocator := context.allocator) -> ^HashTable {
	table := new(HashTable, allocator)
	table.buckets = make([]^Node, size, allocator)
	return table
}

// Create a standalone chain node
create_node :: proc(key: string, value: int, allocator := context.allocator) -> ^Node {
	new_node := new(Node, allocator)
	new_node.key = strings.clone(key, allocator) // Allocates and copies string
	new_node.value = value
	new_node.next = nil
	return new_node
}

// Insert or update key-value pair
insert :: proc(table: ^HashTable, key: string, value: int, allocator := context.allocator) {
	idx := hash(key, len(table.buckets))
	current := table.buckets[idx]

	// Check if key already exists; update if found
	for current != nil {
		if current.key == key {
			current.value = value
			return
		}
		current = current.next
	}

	// Key not found: insert at head of list
	new_node := create_node(key, value, allocator)
	new_node.next = table.buckets[idx]
	table.buckets[idx] = new_node
}

// Search for a key; returns (value, true) if found, (0, false) otherwise
search :: proc(table: ^HashTable, key: string) -> (int, bool) {
	idx := hash(key, len(table.buckets))
	current := table.buckets[idx]

	for current != nil {
		if current.key == key {
			return current.value, true
		}
		current = current.next
	}
	return 0, false
}

// Delete an entry by key
delete_key :: proc(table: ^HashTable, key: string, allocator := context.allocator) -> bool {
	idx := hash(key, len(table.buckets))
	current := table.buckets[idx]
	prev: ^Node = nil

	for current != nil {
		if current.key == key {
			if prev == nil {
				table.buckets[idx] = current.next // Remove head
			} else {
				prev.next = current.next          // Bypass node
			}
			delete(current.key, allocator)
			free(current, allocator)
			return true
		}
		prev = current
		current = current.next
	}
	return false
}

// Display table state bucket by bucket
print_table :: proc(table: ^HashTable) {
	fmt.println("\n--- Hash Table State ---")
	for head, i in table.buckets {
		fmt.printf("Bucket [%d]: ", i)
		if head == nil {
			fmt.println("nil")
			continue
		}

		current := head
		for current != nil {
			fmt.printf("(%s: %d) -> ", current.key, current.value)
			current = current.next
		}
		fmt.println("nil")
	}
}

// Free all dynamically allocated table memory
free_table :: proc(table: ^HashTable, allocator := context.allocator) {
	for head in table.buckets {
		current := head
		for current != nil {
			temp := current
			current = current.next
			delete(temp.key, allocator)
			free(temp, allocator)
		}
	}
	delete(table.buckets, allocator)
	free(table, allocator)
}

main :: proc() {
	table := create_table(TABLE_SIZE)

	// Insert entries
	insert(table, "apple", 100)
	insert(table, "banana", 200)
	insert(table, "cherry", 300)
	insert(table, "date", 400)
	insert(table, "elderberry", 500)

	print_table(table)

	// Lookup (idiomatic comma-ok pattern in Odin)
	if val, ok := search(table, "banana"); ok {
		fmt.printf("\nLookup 'banana': Found value = %d\n", val)
	}

	// Delete
	fmt.println("\nDeleting key 'banana'...")
	delete_key(table, "banana")
	print_table(table)

	// Cleanup memory
	free_table(table)
	fmt.println("\nMemory freed successfully.")
}
