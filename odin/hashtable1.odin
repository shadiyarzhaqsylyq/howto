package main
/*
Hash Table
Open Addressing(Linear Probing)
When collision occurs, the algorithm searches forward sequentially until finds the
first available empty slot and places the new element there. This means early elements
stay close to their original hash slot("rich"), while late-arriving elements can be pushed very 
far away("poor"), leading to high variance in lookup times.

Index   | Status   | Page ID | Frame ID
Slot[0] | Empty    |         | 
Slot[1] | OCCUPIED | 102     | Frame 5
Slot[2] | OCCUPIED | 454     | Frame 12
Slot[3] | OCCUPIED | 15      | Frame 1
Slot[4] | EMPTY    | 
Slot[5] | EMPTY    | 


*/
package main

import "base:intrinsics"
import "core:fmt"
import "core:hash"
import "core:mem"

Slot_State :: enum u8 {
	Empty = 0,
	Occupied,
	Tombstone,
}

Entry :: struct($K, $V: typeid) {
	key:   K,
	value: V,
	state: Slot_State,
}

Hash_Table :: struct($K, $V: typeid) where intrinsics.type_is_comparable(K) {
	entries:    []Entry(K, V),
	count:      int, // Active occupied slots
	tombstones: int, // Deleted slots awaiting reuse or rehash
	allocator:  mem.Allocator,
}

@(private)
next_power_of_two :: proc(n: int) -> int {
	if n <= 8 do return 8
	p := 8
	for p < n {
		p *= 2
	}
	return p
}

@(private)
hash_key :: proc(key: $K) -> u64 {
	when K == string {
		return hash.fnv64a(transmute([]u8)key)
	} else when K == cstring {
		return hash.fnv64a(transmute([]u8)string(key))
	} else {
		k := key
		// Normalize -0.0 to +0.0 so equal float keys hash identically
		when intrinsics.type_is_float(K) {
			if k == 0 do k = 0
		}
		bytes := ([^]u8)(&k)[:size_of(K)]
		return hash.fnv64a(bytes)
	}
}

ht_init :: proc(
	ht: ^Hash_Table($K, $V),
	initial_cap := 8,
	allocator := context.allocator,
) -> mem.Allocator_Error {
	cap := next_power_of_two(initial_cap)
	ht.allocator = allocator
	ht.count = 0
	ht.tombstones = 0
	entries, err := make([]Entry(K, V), cap, allocator)
	if err != nil {
		return err
	}
	ht.entries = entries
	return nil
}

ht_destroy :: proc(ht: ^Hash_Table($K, $V)) {
	delete(ht.entries, ht.allocator)
	ht.entries = nil
	ht.count = 0
	ht.tombstones = 0
}

ht_clear :: proc(ht: ^Hash_Table($K, $V)) {
	mem.zero_slice(ht.entries)
	ht.count = 0
	ht.tombstones = 0
}

@(private)
ht_rehash :: proc(ht: ^Hash_Table($K, $V), new_cap: int) -> mem.Allocator_Error {
	new_cap := next_power_of_two(new_cap)
	new_entries, err := make([]Entry(K, V), new_cap, ht.allocator)
	if err != nil {
		return err
	}

	mask := u64(new_cap - 1)
	for &old in ht.entries {
		if old.state != .Occupied do continue

		idx := int(hash_key(old.key) & mask)
		for new_entries[idx].state == .Occupied {
			idx = (idx + 1) & int(mask)
		}
		new_entries[idx] = Entry(K, V){
			key   = old.key,
			value = old.value,
			state = .Occupied,
		}
	}

	delete(ht.entries, ht.allocator)
	ht.entries = new_entries
	ht.tombstones = 0
	return nil
}

ht_put :: proc(ht: ^Hash_Table($K, $V), key: K, value: V) -> mem.Allocator_Error {
	if len(ht.entries) == 0 {
		alloc := ht.allocator
		if alloc.procedure == nil {
			alloc = context.allocator
		}
		ht_init(ht, 8, alloc) or_return
	} else if (ht.count + ht.tombstones + 1) * 10 > len(ht.entries) * 7 {
		// Grow if active entries >= 50% of capacity; otherwise rehash at same size to clear tombstones
		new_cap := len(ht.entries)
		if ht.count * 2 >= len(ht.entries) {
			new_cap *= 2
		}
		ht_rehash(ht, new_cap) or_return
	}

	mask := u64(len(ht.entries) - 1)
	idx := int(hash_key(key) & mask)
	first_tombstone := -1

	for _ in 0..<len(ht.entries) {
		entry := &ht.entries[idx]
		switch entry.state {
		case .Empty:
			target_idx := idx
			if first_tombstone != -1 {
				target_idx = first_tombstone
				ht.tombstones -= 1
			}
			ht.entries[target_idx] = Entry(K, V){
				key   = key,
				value = value,
				state = .Occupied,
			}
			ht.count += 1
			return nil

		case .Occupied:
			if entry.key == key {
				entry.value = value
				return nil
			}

		case .Tombstone:
			if first_tombstone == -1 {
				first_tombstone = idx
			}
		}
		idx = (idx + 1) & int(mask)
	}

	if first_tombstone != -1 {
		ht.tombstones -= 1
		ht.entries[first_tombstone] = Entry(K, V){
			key   = key,
			value = value,
			state = .Occupied,
		}
		ht.count += 1
	}
	return nil
}

ht_get_ptr :: proc(ht: ^Hash_Table($K, $V), key: K) -> ^V {
	if len(ht.entries) == 0 || ht.count == 0 do return nil

	mask := u64(len(ht.entries) - 1)
	idx := int(hash_key(key) & mask)

	for _ in 0..<len(ht.entries) {
		entry := &ht.entries[idx]
		switch entry.state {
		case .Empty:
			return nil
		case .Occupied:
			if entry.key == key do return &entry.value
		case .Tombstone:
			// Keep probing past deleted slots
		}
		idx = (idx + 1) & int(mask)
	}
	return nil
}

ht_get :: proc(ht: ^Hash_Table($K, $V), key: K) -> (value: V, ok: bool) {
	ptr := ht_get_ptr(ht, key)
	if ptr == nil do return {}, false
	return ptr^, true
}

ht_contains :: proc(ht: ^Hash_Table($K, $V), key: K) -> bool {
	return ht_get_ptr(ht, key) != nil
}

ht_remove :: proc(ht: ^Hash_Table($K, $V), key: K) -> bool {
	if len(ht.entries) == 0 || ht.count == 0 do return false

	mask := u64(len(ht.entries) - 1)
	idx := int(hash_key(key) & mask)

	for _ in 0..<len(ht.entries) {
		entry := &ht.entries[idx]
		switch entry.state {
		case .Empty:
			return false
		case .Occupied:
			if entry.key == key {
				entry.key = {}
				entry.value = {}
				entry.state = .Tombstone
				ht.count -= 1
				ht.tombstones += 1
				return true
			}
		case .Tombstone:
			// Keep probing
		}
		idx = (idx + 1) & int(mask)
	}
	return false
}

main :: proc() {
	ht: Hash_Table(string, int)
	defer ht_destroy(&ht)

	ht_put(&ht, "apple", 10)
	ht_put(&ht, "banana", 20)
	ht_put(&ht, "cherry", 30)
	ht_put(&ht, "apple", 99) // Update existing key

	if val, ok := ht_get(&ht, "apple"); ok {
		fmt.println("apple =", val)
	}

	// Mutate in-place via pointer
	if ptr := ht_get_ptr(&ht, "banana"); ptr != nil {
		ptr^ += 5
	}

	ht_remove(&ht, "banana")
	fmt.println("contains banana?", ht_contains(&ht, "banana"))

	// Re-insert to reuse tombstone or grow
	ht_put(&ht, "date", 40)

	fmt.printf("Count: %d, Capacity: %d\n", ht.count, len(ht.entries))
	for entry in ht.entries {
		if entry.state == .Occupied {
			fmt.printf("  %s -> %d\n", entry.key, entry.value)
		}
	}
}
