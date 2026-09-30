package main

import "core:fmt"
import "core:hash"
import "core:strings"

TABLE_SIZE :: 7

Node :: struct {
	key:   string,
	value: int,
	next:  ^Node,
}

HashTable :: struct {
	buckets: []^Node,
}

// Compute bucket index using the standard library's djb2 implementation
get_bucket_index :: proc(table: ^HashTable, key: string) -> int {
	h := hash.djb2(transmute([]u8)key)
	return int(h) % len(table.buckets)
}

create_table :: proc(size: int, allocator := context.allocator) -> ^HashTable {
	table := new(HashTable, allocator)
	table.buckets = make([]^Node, size, allocator)
	return table
}

create_node :: proc(key: string, value: int, allocator := context.allocator) -> ^Node {
	node := new(Node, allocator)
	node.key = strings.clone(key, allocator)
	node.value = value
	return node
}

insert :: proc(table: ^HashTable, key: string, value: int, allocator := context.allocator) {
	idx := get_bucket_index(table, key)
	current := table.buckets[idx]

	for current != nil {
		if current.key == key {
			current.value = value
			return
		}
		current = current.next
	}

	new_node := create_node(key, value, allocator)
	new_node.next = table.buckets[idx]
	table.buckets[idx] = new_node
}

search :: proc(table: ^HashTable, key: string) -> (int, bool) {
	idx := get_bucket_index(table, key)
	current := table.buckets[idx]

	for current != nil {
		if current.key == key {
			return current.value, true
		}
		current = current.next
	}
	return 0, false
}

delete_key :: proc(table: ^HashTable, key: string, allocator := context.allocator) -> bool {
	idx := get_bucket_index(table, key)
	current := table.buckets[idx]
	prev: ^Node = nil

	for current != nil {
		if current.key == key {
			if prev == nil {
				table.buckets[idx] = current.next
			} else {
				prev.next = current.next
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
	defer free_table(table)

	insert(table, "apple", 100)
	insert(table, "banana", 200)
	insert(table, "cherry", 300)
	insert(table, "date", 400)
	insert(table, "elderberry", 500)

	print_table(table)

	if val, ok := search(table, "banana"); ok {
		fmt.printf("\nLookup 'banana': Found value = %d\n", val)
	}

	fmt.println("\nDeleting key 'banana'...")
	delete_key(table, "banana")
	print_table(table)
}
