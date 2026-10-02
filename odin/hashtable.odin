package main

import "core:fmt"

Page_ID  :: distinct u64
Frame_ID :: distinct u32

Slot_State :: enum u8 {
	EMPTY,
	OCCUPIED,
	TOMBSTONE, // Page was deleted/evicted, probe must continue
}

Bucket :: struct {
	state:    Slot_State,
	page_id:  Page_ID,
	frame_id: Frame_ID,
}

Buffer_Pool_Table :: struct {
	buckets:  []Bucket,
	count:    int,
	capacity: int,
}

// SplitMix64: High-quality, fast hash mixer for 64-bit integers.
// Eliminates clustering when Page IDs are sequential (e.g., 1, 2, 3...).
splitmix64 :: proc(x: u64) -> u64 {
	z := x + 0x9e3779b97f4a7c15
	z = (z ~ (z >> 30)) * 0xbf58476d1ce4e5b9
	z = (z ~ (z >> 27)) * 0x94d049bb133111eb
	return z ~ (z >> 31)
}

create_table :: proc(capacity: int, allocator := context.allocator) -> Buffer_Pool_Table {
	return Buffer_Pool_Table{
		buckets  = make([]Bucket, capacity, allocator),
		count    = 0,
		capacity = capacity,
	}
}

destroy_table :: proc(table: ^Buffer_Pool_Table, allocator := context.allocator) {
	delete(table.buckets, allocator)
}

insert :: proc(table: ^Buffer_Pool_Table, page_id: Page_ID, frame_id: Frame_ID) -> bool {
	if table.count >= table.capacity {
		return false // Table is full (in practice, keep load factor < 70%)
	}

	// Compute start index using SplitMix
	idx := int(splitmix64(u64(page_id)) % u64(table.capacity))
	first_tombstone := -1

	// Linear Probing: probe (idx + i)
	for i in 0 ..< table.capacity {
		curr_idx := (idx + i) % table.capacity
		bucket := &table.buckets[curr_idx]

		switch bucket.state {
		case .EMPTY:
			// Hit empty slot -> key definitely does not exist further ahead.
			// Reuse an earlier tombstone if we saw one, otherwise use this empty slot.
			target_idx := curr_idx
			if first_tombstone != -1 {
				target_idx = first_tombstone
			}

			table.buckets[target_idx] = Bucket{
				state    = .OCCUPIED,
				page_id  = page_id,
				frame_id = frame_id,
			}
			table.count += 1
			return true

		case .TOMBSTONE:
			// Remember the first tombstone we encountered to reuse its slot
			if first_tombstone == -1 {
				first_tombstone = curr_idx
			}

		case .OCCUPIED:
			// Update if page_id already exists in the buffer pool
			if bucket.page_id == page_id {
				bucket.frame_id = frame_id
				return true
			}
		}
	}

	// If we scanned the whole table and found a tombstone, insert there
	if first_tombstone != -1 {
		table.buckets[first_tombstone] = Bucket{
			state    = .OCCUPIED,
			page_id  = page_id,
			frame_id = frame_id,
		}
		table.count += 1
		return true
	}

	return false
}

search :: proc(table: ^Buffer_Pool_Table, page_id: Page_ID) -> (Frame_ID, bool) {
	idx := int(splitmix64(u64(page_id)) % u64(table.capacity))

	for i in 0 ..< table.capacity {
		curr_idx := (idx + i) % table.capacity
		bucket := &table.buckets[curr_idx]

		#partial switch bucket.state {
		case .EMPTY:
			// Stop probing: page is not in the buffer pool
			return 0, false

		case .OCCUPIED:
			if bucket.page_id == page_id {
				return bucket.frame_id, true
			}
		// If it's a TOMBSTONE, do nothing and keep probing forward
		}
	}

	return 0, false
}

delete_page :: proc(table: ^Buffer_Pool_Table, page_id: Page_ID) -> bool {
	idx := int(splitmix64(u64(page_id)) % u64(table.capacity))

	for i in 0 ..< table.capacity {
		curr_idx := (idx + i) % table.capacity
		bucket := &table.buckets[curr_idx]

		#partial switch bucket.state {
		case .EMPTY:
			return false // Not found

		case .OCCUPIED:
			if bucket.page_id == page_id {
				bucket.state = .TOMBSTONE // Evict from hash table
				table.count -= 1
				return true
			}
		}
	}

	return false
}

print_table :: proc(table: ^Buffer_Pool_Table) {
	fmt.println("--- Table State ---")
	for i in 0 ..< table.capacity {
		bucket := table.buckets[i]
		switch bucket.state {
		case .EMPTY:
			fmt.printf("Slot [%2d]: EMPTY\n", i)
		case .TOMBSTONE:
			fmt.printf("Slot [%2d]: [TOMBSTONE]\n", i)
		case .OCCUPIED:
			fmt.printf("Slot [%2d]: Page %-4d -> Frame %d\n", i, bucket.page_id, bucket.frame_id)
		}
	}
	fmt.println("-------------------")
}

main :: proc() {
	// Example: Buffer pool with 7 table slots
	table := create_table(7)
	defer destroy_table(&table)

	// Insert some pages mapped to frame numbers
	insert(&table, Page_ID(10), Frame_ID(0))
	insert(&table, Page_ID(17), Frame_ID(1)) // Likely collides or probes near 10
	insert(&table, Page_ID(99), Frame_ID(2))

	print_table(&table)

	// Lookup Page 17
	if frame, ok := search(&table, Page_ID(17)); ok {
		fmt.printf("\nFound Page 17 in Frame %d\n\n", frame)
	}

	// Evict / Remove Page 10
	fmt.println("Evicting Page 10...")
	delete_page(&table, Page_ID(10))
	print_table(&table)

	// Search for Page 17 again (verifies that probe walks PAST the tombstone)
	if frame, ok := search(&table, Page_ID(17)); ok {
		fmt.printf("\nSuccessfully bypassed tombstone: Page 17 is in Frame %d\n", frame)
	}
}
