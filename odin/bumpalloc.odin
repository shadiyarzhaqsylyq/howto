package main

import "core:fmt"
import "core:mem"

// ---------------------------------------------------------
// 1. Core Bump Allocator State & Logic
// ---------------------------------------------------------

Bump_Allocator :: struct {
	data:   []byte,
	offset: int,
}

bump_init :: proc(arena: ^Bump_Allocator, backing_buffer: []byte) {
	arena.data = backing_buffer
	arena.offset = 0
}

bump_alloc :: proc(arena: ^Bump_Allocator, size, alignment: int) -> ([]byte, mem.Allocator_Error) {
	if size <= 0 {
		return nil, .None
	}

	curr_ptr := uintptr(raw_data(arena.data)) + uintptr(arena.offset)
	aligned_ptr := mem.align_forward_uintptr(curr_ptr, uintptr(alignment))

	new_offset := int(aligned_ptr - uintptr(raw_data(arena.data))) + size
	if new_offset > len(arena.data) {
		return nil, .Out_Of_Memory
	}

	arena.offset = new_offset
	return mem.byte_slice(rawptr(aligned_ptr), size), .None
}

bump_reset :: proc(arena: ^Bump_Allocator) {
	arena.offset = 0
}

// ---------------------------------------------------------
// 2. Standard Odin Allocator Wrapper
// ---------------------------------------------------------

bump_allocator_proc :: proc(
	allocator_data: rawptr,
	mode: mem.Allocator_Mode,
	size, alignment: int,
	old_memory: rawptr,
	old_size: int,
	loc := #caller_location,
) -> ([]byte, mem.Allocator_Error) {
	arena := (^Bump_Allocator)(allocator_data)

	#partial switch mode {
	case .Alloc:
		bytes, err := bump_alloc(arena, size, alignment)
		if err == .None && len(bytes) > 0 {
			// .Alloc requires zero-initialized memory
			mem.zero_slice(bytes)
		}
		return bytes, err

	case .Alloc_Non_Zeroed:
		return bump_alloc(arena, size, alignment)

	case .Free_All:
		bump_reset(arena)
		return nil, .None

	case .Resize, .Resize_Non_Zeroed:
		// Optional optimization: if the old memory is the last allocation made,
		// we can expand or shrink it in place!
		curr_ptr := uintptr(raw_data(arena.data)) + uintptr(arena.offset)
		if uintptr(old_memory) + uintptr(old_size) == curr_ptr {
			new_offset := arena.offset - old_size + size
			if new_offset <= len(arena.data) {
				arena.offset = new_offset
				return mem.byte_slice(old_memory, size), .None
			}
		}

		// Otherwise, allocate fresh memory and copy the old contents over
		new_bytes, err := bump_alloc(arena, size, alignment)
		if err != .None {
			return nil, err
		}
		copy_size := min(old_size, size)
		if copy_size > 0 && old_memory != nil {
			mem.copy(raw_data(new_bytes), old_memory, copy_size)
		}
		return new_bytes, .None

	case .Free:
		// Bump allocators do not support individual frees (no-op)
		return nil, .None

	case .Query_Features:
		set := (^mem.Allocator_Mode_Set)(old_memory)
		if set != nil {
			set^ = {
				.Alloc,
				.Alloc_Non_Zeroed,
				.Free,
				.Free_All,
				.Resize,
				.Resize_Non_Zeroed,
				.Query_Features,
			}
		}
		return nil, .None

	case .Query_Info:
		return nil, .Mode_Not_Implemented
	}

	return nil, .Mode_Not_Implemented
}

// Converts a Bump_Allocator pointer into a standard mem.Allocator interface
bump_allocator :: proc(arena: ^Bump_Allocator) -> mem.Allocator {
	return mem.Allocator{
		procedure = bump_allocator_proc,
		data      = arena,
	}
}

// ---------------------------------------------------------
// 3. Example Usage
// ---------------------------------------------------------

Player :: struct {
	name:   string,
	health: int,
	pos:    [3]f32,
}

main :: proc() {
	// Pre-allocate a 64 KB backing buffer
	backing_buffer: [64 * mem.Kilobyte]byte

	arena: Bump_Allocator
	bump_init(&arena, backing_buffer[:])

	// Create the standard allocator interface
	alloc := bump_allocator(&arena)

	// --- A. Using built-in new() and make() ---
	player := new(Player, alloc)
	player.name = "Arthur"
	player.health = 100
	player.pos = {0.0, 1.5, -4.0}

	numbers := make([]int, 5, alloc)
	for i in 0 ..< len(numbers) {
		numbers[i] = (i + 1) * 10
	}

	fmt.printf("Player: %s (HP: %d)\n", player.name, player.health)
	fmt.printf("Numbers slice: %v\n", numbers)
	fmt.printf("Arena used so far: %d bytes\n\n", arena.offset)

	// --- B. Using implicit context.allocator ---
	{
		// Any allocations within this block use our bump allocator by default
		context.allocator = alloc

		// 1. Dynamic arrays (resizing works through .Resize!)
		list := make([dynamic]int)
		for i in 1 ..= 10 {
			append(&list, i * i)
		}
		fmt.printf("Dynamic Array: %v\n", list[:])

		// 2. Formatted string allocation (fmt.aprintf uses context.allocator)
		msg := fmt.aprintf("Score: %d | Level: %s", 9990, "Castle")
		fmt.println(msg)

		// 3. Hash Maps
		scores := make(map[string]int)
		scores["Alice"] = 95
		scores["Bob"] = 88
		fmt.printf("Map: Alice -> %d, Bob -> %d\n", scores["Alice"], scores["Bob"])
	}

	fmt.printf("\nArena used before reset: %d / %d bytes\n", arena.offset, len(arena.data))

	// --- C. Reset everything in one go ---
	free_all(alloc)
	fmt.printf("Arena used after free_all: %d bytes\n", arena.offset)
}
