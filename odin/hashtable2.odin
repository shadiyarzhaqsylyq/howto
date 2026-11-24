/* RobinHood Hashing
It uses the same sequential search but tracks each element's Probe Sequence Length(PSL).
Probe Sequence Length(PSL) is the distance an item has traveled from its initial hash slot.
If a new element is being inserted and encounters an existing element that has a shorter PSL,
the new element "steals" the slot and bumps the existing element out. The bumped element then
continues probing down the line. The RobinHood also supports a "backshift deletion"
variant in which when deleting an element you backshift the elements that were displaced by probing
to get them closer to their intended spot, reducing the distance(and obviating the need for tombstones).
*/
package main

import "base:intrinsics"
import "core:fmt"
import "core:hash"
import "core:mem"

Entry :: struct($K, $V: typeid) {
	key:   K,
	value: V,
	psl:   u32, // 0 = empty; >= 1 = occupied (distance from ideal slot is psl - 1)
}

Robin_Hood_Table :: struct($K, $V: typeid) where intrinsics.type_is_comparable(K) {
	entries:   []Entry(K, V),
	count:     int,
	allocator: mem.Allocator,
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
		when intrinsics.type_is_float(K) {
			if k == 0 do k = 0
		}
		bytes := ([^]u8)(&k)[:size_of(K)]
		return hash.fnv64a(bytes)
	}
}

rh_init :: proc(
	ht: ^Robin_Hood_Table($K, $V),
	initial_cap := 8,
	allocator := context.allocator,
) -> mem.Allocator_Error {
	cap := next_power_of_two(initial_cap)
	ht.allocator = allocator
	ht.count = 0
	entries, err := make([]Entry(K, V), cap, allocator)
	if err != nil {
		return err
	}
	ht.entries = entries
	return nil
}

rh_destroy :: proc(ht: ^Robin_Hood_Table($K, $V)) {
	delete(ht.entries, ht.allocator)
	ht.entries = nil
	ht.count = 0
}

rh_clear :: proc(ht: ^Robin_Hood_Table($K, $V)) {
	mem.zero_slice(ht.entries)
	ht.count = 0
}

@(private)
rh_rehash :: proc(ht: ^Robin_Hood_Table($K, $V), new_cap: int) -> mem.Allocator_Error {
	new_cap := next_power_of_two(new_cap)
	new_entries, err := make([]Entry(K, V), new_cap, ht.allocator)
	if err != nil {
		return err
	}

	mask := u64(new_cap - 1)
	for &old in ht.entries {
		if old.psl == 0 do continue

		incoming := Entry(K, V){
			key   = old.key,
			value = old.value,
			psl   = 1,
		}
		idx := int(hash_key(incoming.key) & mask)

		for {
			entry := &new_entries[idx]
			if entry.psl == 0 {
				entry^ = incoming
				break
			}
			if incoming.psl > entry.psl {
				incoming, entry^ = entry^, incoming
			}
			incoming.psl += 1
			idx = (idx + 1) & int(mask)
		}
	}

	delete(ht.entries, ht.allocator)
	ht.entries = new_entries
	return nil
}

rh_put :: proc(ht: ^Robin_Hood_Table($K, $V), key: K, value: V) -> mem.Allocator_Error {
	if len(ht.entries) == 0 {
		alloc := ht.allocator
		if alloc.procedure == nil {
			alloc = context.allocator
		}
		rh_init(ht, 8, alloc) or_return
	} else if (ht.count + 1) * 4 > len(ht.entries) * 3 {
		// Grow at 75% load factor (no tombstones to worry about)
		rh_rehash(ht, len(ht.entries) * 2) or_return
	}

	incoming := Entry(K, V){
		key   = key,
		value = value,
		psl   = 1,
	}

	mask := u64(len(ht.entries) - 1)
	idx := int(hash_key(key) & mask)
	swapped := false

	for _ in 0..<len(ht.entries) {
		entry := &ht.entries[idx]

		// Empty slot found
		if entry.psl == 0 {
			entry^ = incoming
			ht.count += 1
			return nil
		}

		// Update existing key (cannot happen once a Robin Hood swap has occurred)
		if !swapped && entry.key == incoming.key {
			entry.value = incoming.value
			return nil
		}

		// Rob from the rich: steal the slot if incoming traveled farther
		if incoming.psl > entry.psl {
			incoming, entry^ = entry^, incoming
			swapped = true
		}

		incoming.psl += 1
		idx = (idx + 1) & int(mask)
	}

	return nil
}

rh_get_ptr :: proc(ht: ^Robin_Hood_Table($K, $V), key: K) -> ^V {
	if len(ht.entries) == 0 || ht.count == 0 do return nil

	mask := u64(len(ht.entries) - 1)
	idx := int(hash_key(key) & mask)
	psl: u32 = 1

	for _ in 0..<len(ht.entries) {
		entry := &ht.entries[idx]

		// Early exit: if slot is empty OR occupant is closer to home than our probe,
		// the key cannot exist further down the chain.
		if entry.psl == 0 || psl > entry.psl {
			return nil
		}

		if entry.key == key {
			return &entry.value
		}

		psl += 1
		idx = (idx + 1) & int(mask)
	}
	return nil
}

rh_get :: proc(ht: ^Robin_Hood_Table($K, $V), key: K) -> (value: V, ok: bool) {
	ptr := rh_get_ptr(ht, key)
	if ptr == nil do return {}, false
	return ptr^, true
}

rh_contains :: proc(ht: ^Robin_Hood_Table($K, $V), key: K) -> bool {
	return rh_get_ptr(ht, key) != nil
}

rh_remove :: proc(ht: ^Robin_Hood_Table($K, $V), key: K) -> bool {
	if len(ht.entries) == 0 || ht.count == 0 do return false

	mask := u64(len(ht.entries) - 1)
	idx := int(hash_key(key) & mask)
	psl: u32 = 1

	for _ in 0..<len(ht.entries) {
		entry := &ht.entries[idx]
		if entry.psl == 0 || psl > entry.psl {
			return false
		}

		if entry.key == key {
			// Backward-shift deletion: pull subsequent displaced entries back by 1
			for {
				next_idx := (idx + 1) & int(mask)
				next_entry := &ht.entries[next_idx]
				if next_entry.psl <= 1 {
					// Stop when hitting an empty slot (0) or an entry in its ideal slot (1)
					ht.entries[idx] = {}
					break
				}
				ht.entries[idx] = next_entry^
				ht.entries[idx].psl -= 1
				idx = next_idx
			}
			ht.count -= 1
			return true
		}

		psl += 1
		idx = (idx + 1) & int(mask)
	}
	return false
}

main :: proc() {
	ht: Robin_Hood_Table(string, int)
	defer rh_destroy(&ht)

	words := []string{"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta"}
	for w, i in words {
		rh_put(&ht, w, (i + 1) * 10)
	}

	rh_put(&ht, "gamma", 999) // Update
	rh_remove(&ht, "beta")    // Backward-shift delete (no tombstones)

	if val, ok := rh_get(&ht, "gamma"); ok {
		fmt.println("gamma =", val)
	}
	fmt.println("contains beta?", rh_contains(&ht, "beta"))

	fmt.printf("Count: %d, Capacity: %d\n", ht.count, len(ht.entries))
	for entry, slot in ht.entries {
		if entry.psl > 0 {
			fmt.printf("  slot %2d: %-8s -> %3d (probe dist: %d)\n",
				slot, entry.key, entry.value, entry.psl - 1)
		}
	}
}
