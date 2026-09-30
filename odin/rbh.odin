package main

import "core:fmt"
import "core:hash"

// Identifiers used in Buffer Pools
Page_ID  :: distinct u64
Frame_ID :: distinct u32

// Special constant representing an unoccupied slot
EMPTY_PAGE: Page_ID : ~Page_ID(0) // 0xFFFFFFFFFFFFFFFF

Slot :: struct {
	page_id:  Page_ID,
	frame_id: Frame_ID,
	psl:      u32, // Probe Sequence Length (Distance from ideal home)
}

RobinHoodTable :: struct {
	slots: []Slot,
	count: int,
	mask:  u64, // (capacity - 1), capacity MUST be a power of two
}

// Initialize a Robin Hood Table.
// Note: initial_capacity must be a power of two (e.g., 1024, 2048, 65536).
init_table :: proc(capacity: int, allocator := context.allocator) -> RobinHoodTable {
	assert(capacity > 0 && (capacity & (capacity - 1)) == 0, "Capacity must be a power of two!")

	table: RobinHoodTable
	table.slots = make([]Slot, capacity, allocator)
	table.count = 0
	table.mask  = u64(capacity - 1)

	for i in 0 ..< capacity {
		table.slots[i].page_id = EMPTY_PAGE
		table.slots[i].psl = 0
	}

	return table
}

destroy_table :: proc(table: ^RobinHoodTable, allocator := context.allocator) {
	delete(table.slots, allocator)
}

// Fast integer hash for 64-bit page IDs (SplitMix64)
hash_page_id :: proc(page_id: Page_ID) -> u64 {
	x := u64(page_id)
	x = (x ~ (x >> 30)) * 0xbf58476d1ce4e5b9
	x = (x ~ (x >> 27)) * 0x94d049bb133111eb
	x = x ~ (x >> 31)
	return x
}

// ----------------------------------------------------------------------------
// 1. LOOKUP (Search)
// ----------------------------------------------------------------------------
get :: proc(table: ^RobinHoodTable, page_id: Page_ID) -> (Frame_ID, bool) {
	idx := hash_page_id(page_id) & table.mask
	current_psl: u32 = 0

	for {
		slot := &table.slots[idx]

		// 1. Hit an empty slot -> page definitely not in table
		if slot.page_id == EMPTY_PAGE {
			return 0, false
		}

		// 2. Found the key -> Cache hit!
		if slot.page_id == page_id {
			return slot.frame_id, true
		}

		// 3. Early termination: "Rich" element encountered!
		// If the resident's PSL is strictly smaller than our current PSL,
		// the key would have swapped places here if it existed.
		if current_psl > slot.psl {
			return 0, false
		}

		// Step to next slot (linear probe)
		current_psl += 1
		idx = (idx + 1) & table.mask
	}
}

// ----------------------------------------------------------------------------
// 2. INSERT (Steal from the rich, give to the poor)
// ----------------------------------------------------------------------------
insert :: proc(table: ^RobinHoodTable, page_id: Page_ID, frame_id: Frame_ID) -> bool {
	// Guard against 100% full table
	if table.count >= len(table.slots) {
		return false
	}

	idx := hash_page_id(page_id) & table.mask
	entry := Slot{
		page_id  = page_id,
		frame_id = frame_id,
		psl      = 0,
	}

	for {
		slot := &table.slots[idx]

		// Scenario A: Found an empty slot -> Claim it
		if slot.page_id == EMPTY_PAGE {
			slot^ = entry
			table.count += 1
			return true
		}

		// Scenario B: Key already exists -> Update value
		if slot.page_id == entry.page_id {
			slot.frame_id = entry.frame_id
			return true
		}

		// Scenario C: Incoming item is "poorer" than the current resident -> SWAP!
		// The richer resident is displaced and becomes the item to insert.
		if entry.psl > slot.psl {
			// Swap current entry with slot
			temp := slot^
			slot^ = entry
			entry = temp
		}

		// Step forward with whichever entry is looking for a home
		entry.psl += 1
		idx = (idx + 1) & table.mask
	}
}

// ----------------------------------------------------------------------------
// 3. DELETE (Backward-Shift Deletion — No tombstones!)
// ----------------------------------------------------------------------------
delete_key :: proc(table: ^RobinHoodTable, page_id: Page_ID) -> bool {
	idx := hash_page_id(page_id) & table.mask
	current_psl: u32 = 0

	// 1. Locate the entry to delete
	for {
		slot := &table.slots[idx]

		if slot.page_id == EMPTY_PAGE || current_psl > slot.psl {
			return false // Not found
		}

		if slot.page_id == page_id {
			break // Found at `idx`
		}

		current_psl += 1
		idx = (idx + 1) & table.mask
	}

	// 2. Backward-shift subsequent slots to fill the hole cleanly
	table.count -= 1
	for {
		next_idx := (idx + 1) & table.mask
		next_slot := &table.slots[next_idx]

		// Stop if next slot is empty or already in its ideal bucket (PSL == 0)
		if next_slot.page_id == EMPTY_PAGE || next_slot.psl == 0 {
			table.slots[idx].page_id = EMPTY_PAGE
			table.slots[idx].psl = 0
			break
		}

		// Shift next slot backward by 1 position and decrement its PSL
		table.slots[idx] = next_slot^
		table.slots[idx].psl -= 1

		idx = next_idx
	}

	return true
}

// ----------------------------------------------------------------------------
// Demo
// ----------------------------------------------------------------------------
main :: proc() {
	// Allocate a small table of size 8 for clear visibility
	table := init_table(8)
	defer destroy_table(&table)

	fmt.println("--- Inserting Pages ---")
	insert(&table, Page_ID(10), Frame_ID(0))
	insert(&table, Page_ID(18), Frame_ID(1)) // Intentionally collides with 10 on low bits
	insert(&table, Page_ID(26), Frame_ID(2)) // Collides further
	insert(&table, Page_ID(42), Frame_ID(3))

	for slot, i in table.slots {
		if slot.page_id == EMPTY_PAGE {
			fmt.printf("Slot [%d]: EMPTY\n", i)
		} else {
			fmt.printf("Slot [%d]: Page %v -> Frame %v (PSL: %d)\n", i, slot.page_id, slot.frame_id, slot.psl)
		}
	}

	// Lookup test
	fmt.println("\n--- Testing Lookups ---")
	if fid, ok := get(&table, Page_ID(18)); ok {
		fmt.printf("Found Page 18 at Frame %d\n", fid)
	}

	// Early abort test (non-existent key)
	if _, ok := get(&table, Page_ID(999)); !ok {
		fmt.println("Page 999 not found (Early abort worked!)")
	}

	// Backward-shift deletion test
	fmt.println("\n--- Deleting Page 18 ---")
	delete_key(&table, Page_ID(18))

	for slot, i in table.slots {
		if slot.page_id == EMPTY_PAGE {
			fmt.printf("Slot [%d]: EMPTY\n", i)
		} else {
			fmt.printf("Slot [%d]: Page %v -> Frame %v (PSL: %d)\n", i, slot.page_id, slot.frame_id, slot.psl)
		}
	}
}
