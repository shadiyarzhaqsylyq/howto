package main
/*
Hash Table
Open Addressing(Linear Probing)
When collision occurs, the algorithm searches forward sequentially until finds the
first available empty slot and places the new element there. This means early elements
stay close to their original hash slot("rich"), while late-arriving elements can be pushed very 
far away("poor"), leading to high variance in lookup times.

Index, Status, Page ID, Frame ID
Slot[0], OCCUPIED, 102, Frame 5
Slot[1], OCCUPIED, 454, Frame 12
Slot[2], OCCUPIED, 15, Frame 1
Slot[0], EMPTY, ..., ...
Slot[0], EMPTY, ..., ...
*/
import "core:fmt"

Page_ID  :: distinct u64
Frame_ID :: distinct u32

Slot_State :: enum u8 {
	EMPTY,
	OCCUPIED,
	TOMBSTONE, // Retained so probing does not break after deletions
}

Bucket :: struct {
	state:    Slot_State,
	page_id:  Page_ID,
	frame_id: Frame_ID,
}

Buffer_Pool_Table :: struct {
	buckets: []Bucket,
	count:   int,
	mask:    int, // (capacity - 1), used for fast bitwise indexing
}

// SplitMix64: High-speed 64-bit mixer that avalanches consecutive/incremental Page IDs
splitmix64 :: proc(x: u64) -> u64 {
	z := x + 0x9e3779b97f4a7c15
	z = (z ~ (z >> 30)) * 0xbf58476d1ce4e5b9
	z = (z ~ (z >> 27)) * 0x94d049bb133111eb
	return z ~ (z >> 31)
}

// Creates the hash table. Capacity MUST be a power of 2 (e.g., 8, 16, 64, 1024...).
create_table :: proc(capacity: int, allocator := context.allocator) -> Buffer_Pool_Table {
	assert(
		capacity > 0 && (capacity & (capacity - 1)) == 0,
		"Capacity must be a valid power of 2!",
	)

	return Buffer_Pool_Table{
		buckets = make([]Bucket, capacity, allocator),
		count   = 0,
		mask    = capacity - 1,
	}
}

destroy_table :: proc(table: ^Buffer_Pool_Table, allocator := context.allocator) {
	delete(table.buckets, allocator)
	table^ = {}
}

insert :: proc(table: ^Buffer_Pool_Table, page_id: Page_ID, frame_id: Frame_ID) -> bool {
	// Table is full (keep load factor under ~75% in real systems)
	if table.count >= len(table.buckets) {
		return false
	}

	mask := table.mask
	curr_idx := int(splitmix64(u64(page_id))) & mask
	first_tombstone := -1

	for _ in 0 ..= mask {
		bucket := &table.buckets[curr_idx]

		switch bucket.state {
		case .EMPTY:
			// Hit an empty slot: key does not exist further in the probe sequence.
			target_idx := curr_idx
			if first_tombstone != -1 {
				target_idx = first_tombstone // Prioritize recycling an existing tombstone
			}

			table.buckets[target_idx] = Bucket{
				state    = .OCCUPIED,
				page_id  = page_id,
				frame_id = frame_id,
			}
			table.count += 1
			return true

		case .TOMBSTONE:
			// Remember the first tombstone encountered for reuse
			if first_tombstone == -1 {
				first_tombstone = curr_idx
			}

		case .OCCUPIED:
			// Page is already present in the buffer pool, update frame mapping
			if bucket.page_id == page_id {
				bucket.frame_id = frame_id
				return true
			}
		}

		// Fast bitwise probe advance & wrap-around
		curr_idx = (curr_idx + 1) & mask
	}

	// If we looped around and found an earlier tombstone, insert there
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
	mask := table.mask
	curr_idx := int(splitmix64(u64(page_id))) & mask

	for _ in 0 ..= mask {
		bucket := &table.buckets[curr_idx]

		#partial switch bucket.state {
		case .EMPTY:
			return 0, false // Page not in buffer pool

		case .OCCUPIED:
			if bucket.page_id == page_id {
				return bucket.frame_id, true // Page hit!
			}
		// Skip TOMBSTONE and continue linear probing
		}

		curr_idx = (curr_idx + 1) & mask
	}

	return 0, false
}

delete_page :: proc(table: ^Buffer_Pool_Table, page_id: Page_ID) -> bool {
	mask := table.mask
	curr_idx := int(splitmix64(u64(page_id))) & mask

	for _ in 0 ..= mask {
		bucket := &table.buckets[curr_idx]

		#partial switch bucket.state {
		case .EMPTY:
			return false // Not found

		case .OCCUPIED:
			if bucket.page_id == page_id {
				bucket.state = .TOMBSTONE // Mark slot as evicted
				table.count -= 1
				return true
			}
		}

		curr_idx = (curr_idx + 1) & mask
	}

	return false
}

print_table :: proc(table: ^Buffer_Pool_Table) {
	fmt.println("\n--- Buffer Pool Hash Table ---")
	for i in 0 ..< len(table.buckets) {
		bucket := table.buckets[i]
		switch bucket.state {
		case .EMPTY:
			fmt.printf("Slot [%2d]: EMPTY\n", i)
		case .TOMBSTONE:
			fmt.printf("Slot [%2d]: [TOMBSTONE]\n", i)
		case .OCCUPIED:
			fmt.printf("Slot [%2d]: OCCUPIED (Page %-3d -> Frame %d)\n", i, bucket.page_id, bucket.frame_id)
		}
	}
	fmt.printf("Total Count: %d / %d\n", table.count, len(table.buckets))
	fmt.println("------------------------------")
}

main :: proc() {
	// 1. Capacity must be a power of 2 (8 slots)
	table := create_table(8)
	defer destroy_table(&table)

	// 2. Insert sequential Page IDs (common in table scans)
	insert(&table, Page_ID(100), Frame_ID(0))
	insert(&table, Page_ID(101), Frame_ID(1))
	insert(&table, Page_ID(102), Frame_ID(2))
	insert(&table, Page_ID(103), Frame_ID(3))

	print_table(&table)

	// 3. Search for a page
	target_page := Page_ID(102)
	if frame, ok := search(&table, target_page); ok {
		fmt.printf("\nHit! Page %d is stored in Frame %d\n", target_page, frame)
	}

	// 4. Evict a page (transforms slot to TOMBSTONE)
	fmt.printf("\nEvicting Page 101 from Buffer Pool...\n")
	delete_page(&table, Page_ID(101))
	print_table(&table)

	// 5. Lookup page 102 again (verifies linear probe bypasses the tombstone)
	if frame, ok := search(&table, target_page); ok {
		fmt.printf("\nSuccessfully bypassed tombstone: Page %d is still in Frame %d\n", target_page, frame)
	}

	// 6. Insert a new page (verifies tombstone reuse)
	fmt.printf("\nInserting Page 200 (recycles first available tombstone or empty slot)...\n")
	insert(&table, Page_ID(200), Frame_ID(7))
	print_table(&table)
}
