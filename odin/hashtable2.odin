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

import "core:fmt"
import "core:mem"

PageID  :: distinct u64
FrameID :: distinct u32

INVALID_PAGE_ID:  PageID  : ~PageID(0)
INVALID_FRAME_ID: FrameID : ~FrameID(0)
MAX_LOAD_FACTOR           : f64 : 0.75

// Bit-packing utilities
make_page_id :: #force_inline proc(table_id: u32, page_num: u32) -> PageID {
	return PageID((u64(table_id) << 32) | u64(page_num))
}

get_table_id :: #force_inline proc(id: PageID) -> u32 {
	return u32(u64(id) >> 32)
}

get_page_num :: #force_inline proc(id: PageID) -> u32 {
	return u32(id)
}

// Rounds up to the nearest power of 2
next_pow2 :: proc(n: uint) -> uint {
	if n <= 16 do return 16
	x := n - 1
	x |= x >> 1
	x |= x >> 2
	x |= x >> 4
	x |= x >> 8
	x |= x >> 16
	x |= x >> 32
	return x + 1
}
//Alternative
package main

import "core:math/bits"

// 2 * size_of(rawptr) evaluates to 16 on 64-bit platforms
DEFAULT_ALIGNMENT :: 2 * size_of(rawptr)

is_power_of_two :: proc(x: uintptr) -> bool {
	// Odin handles bitwise operations natively on uintptr
	return x > 0 && (x & (x - 1)) == 0
}

align_forward :: proc(ptr: uintptr, align: uintptr) -> uintptr {
	// Replacing asserts: Odin has a built-in 'assert' in the runtime
	assert(is_power_of_two(align), "Alignment must be a power of two")
	
	// Ultra-fast branchless math: rounds up to the next multiple of 'align'
	return (ptr + align - 1) & ~(align - 1)
}
// Alternative

// 64-bit SplitMix64 Hash Mixer
hash_page_id :: #force_inline proc(key: PageID) -> u64 {
	x := u64(key)
	x = (x ~ (x >> 30)) * 0xbf58476d1ce4e5b9
	x = (x ~ (x >> 27)) * 0x94d049bb133111eb
	x = x ~ (x >> 31)
	return x
}

// 16-byte slot: 4 slots align perfectly to 64-byte CPU cache lines
HashSlot :: struct {
	page_id:  PageID,  // 8 bytes
	frame_id: FrameID, // 4 bytes
	dib:      u32,     // 4 bytes: Distance from Initial Bucket
}

PageTable :: struct {
	slots:     []HashSlot,
	capacity:  uint, // Always a power of 2
	mask:      uint, // capacity - 1
	count:     uint,
	allocator: mem.Allocator,
}

// Helper for allocating 64-byte cache-aligned slot slices
allocate_slots :: proc(capacity: uint, allocator := context.allocator) -> []HashSlot {
	// Allocate a slice with 64-byte alignment to match CPU cache lines
	slots, err := mem.make_aligned([]HashSlot, int(capacity), 64, allocator)
	if err != nil do return nil

	for i in 0 ..< capacity {
		slots[i] = HashSlot{
			page_id  = INVALID_PAGE_ID,
			frame_id = INVALID_FRAME_ID,
			dib      = 0,
		}
	}
	return slots
}

free_slots :: proc(slots: []HashSlot, allocator := context.allocator) {
	delete(slots, allocator)
}

page_table_create :: proc(initial_capacity: uint, allocator := context.allocator) -> ^PageTable {
	pt := new(PageTable, allocator)
	if pt == nil do return nil

	pt.allocator = allocator
	pt.capacity = next_pow2(initial_capacity)
	pt.mask = pt.capacity - 1
	pt.count = 0
	pt.slots = allocate_slots(pt.capacity, allocator)

	if pt.slots == nil {
		free(pt, allocator)
		return nil
	}

	return pt
}

page_table_destroy :: proc(pt: ^PageTable) {
	if pt == nil do return
	allocator := pt.allocator
	free_slots(pt.slots, allocator)
	free(pt, allocator)
}

// LOOKUP: O(1) average, with early-exit on smaller DIB
page_table_lookup :: proc(pt: ^PageTable, page_id: PageID) -> (FrameID, bool) {
	idx := uint(hash_page_id(page_id)) & pt.mask
	cur_dib: u32 = 0

	for {
		slot := &pt.slots[idx]

		if slot.page_id == INVALID_PAGE_ID || slot.dib < cur_dib {
			return 0, false // Not found or passed insertion point
		}

		if slot.page_id == page_id {
			return slot.frame_id, true
		}

		cur_dib += 1
		idx = (idx + 1) & pt.mask
	}
}

// Internal raw insertion helper (without load factor check)
page_table_insert_raw :: proc(slots: []HashSlot, mask: uint, incoming: HashSlot) {
	inc := incoming
	idx := uint(hash_page_id(inc.page_id)) & mask

	for {
		slot := &slots[idx]

		if slot.page_id == INVALID_PAGE_ID {
			slot^ = inc
			return
		}

		if slot.page_id == inc.page_id {
			slot.frame_id = inc.frame_id
			return
		}

		// Steal from the rich
		if inc.dib > slot.dib {
			temp := slot^
			slot^ = inc
			inc = temp
		}

		inc.dib += 1
		idx = (idx + 1) & mask
	}
}

// Automatically double table capacity when load factor threshold is met
page_table_resize :: proc(pt: ^PageTable, new_capacity: uint) -> bool {
	new_slots := allocate_slots(new_capacity, pt.allocator)
	if new_slots == nil do return false

	new_mask := new_capacity - 1

	// Rehash and migrate existing slots
	for i in 0 ..< pt.capacity {
		if pt.slots[i].page_id != INVALID_PAGE_ID {
			slot := pt.slots[i]
			slot.dib = 0 // Reset DIB for new hash placement
			page_table_insert_raw(new_slots, new_mask, slot)
		}
	}

	free_slots(pt.slots, pt.allocator)
	pt.slots = new_slots
	pt.capacity = new_capacity
	pt.mask = new_mask

	return true
}

// INSERT / UPDATE
page_table_insert :: proc(pt: ^PageTable, page_id: PageID, frame_id: FrameID) -> bool {
	// 1. If key already exists, update in-place without triggering resize
	if _, ok := page_table_lookup(pt, page_id); ok {
		idx := uint(hash_page_id(page_id)) & pt.mask
		for pt.slots[idx].page_id != page_id {
			idx = (idx + 1) & pt.mask
		}
		pt.slots[idx].frame_id = frame_id
		return true
	}

	// 2. Check load factor threshold (grow table before inserting if needed)
	if f64(pt.count + 1) / f64(pt.capacity) > MAX_LOAD_FACTOR {
		if !page_table_resize(pt, pt.capacity * 2) {
			return false // Re-allocation failed
		}
	}

	// 3. Insert new item
	incoming := HashSlot{
		page_id  = page_id,
		frame_id = frame_id,
		dib      = 0,
	}

	page_table_insert_raw(pt.slots, pt.mask, incoming)
	pt.count += 1
	return true
}

// DELETE: Backward-shift deletion (avoids tombstones)
page_table_delete :: proc(pt: ^PageTable, page_id: PageID) -> bool {
	idx := uint(hash_page_id(page_id)) & pt.mask
	cur_dib: u32 = 0

	// Search for key
	for {
		slot := &pt.slots[idx]
		if slot.page_id == INVALID_PAGE_ID || slot.dib < cur_dib {
			return false // Key not present
		}
		if slot.page_id == page_id {
			break
		}
		cur_dib += 1
		idx = (idx + 1) & pt.mask
	}

	// Backward shift subsequent items to fill the gap
	for {
		next_idx := (idx + 1) & pt.mask
		next_slot := &pt.slots[next_idx]

		if next_slot.page_id == INVALID_PAGE_ID || next_slot.dib == 0 {
			pt.slots[idx].page_id = INVALID_PAGE_ID
			pt.slots[idx].frame_id = INVALID_FRAME_ID
			pt.slots[idx].dib = 0
			break
		}

		pt.slots[idx] = next_slot^
		pt.slots[idx].dib -= 1
		idx = next_idx
	}

	pt.count -= 1
	return true
}

main :: proc() {
	pt := page_table_create(16)
	defer page_table_destroy(pt)

	// Insert elements to trigger auto-resizing
	for i in 0 ..< u32(50) {
		pid := make_page_id(1, i)
		page_table_insert(pt, pid, FrameID(i * 10))
	}

	fmt.printf("Table count: %d, capacity: %d\n", pt.count, pt.capacity)

	target := make_page_id(1, 25)
	if frame, ok := page_table_lookup(pt, target); ok {
		fmt.printf("Page 25 found at Frame %d\n", frame)
	}

	// Delete item
	page_table_delete(pt, target)
	if _, ok := page_table_lookup(pt, target); !ok {
		fmt.println("Page 25 successfully deleted.")
	}
}
