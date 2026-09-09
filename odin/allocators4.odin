package main

import "core:fmt"
import "core:mem"
import "core:mem/virtual"

main :: proc() {
    // 1. Get raw virtual memory from the OS (no heap/malloc involved)
    size := uint(64 * mem.Megabyte)
    buf, err := virtual.reserve_and_commit(size) //or 64 * mem.Megabyte
    if err != nil {
        panic("Failed to reserve virtual memory")
    }
    defer virtual.release(raw_data(buf), len(buf))

    // 2. You can feed that buffer into ANY core:mem allocator:

    // --- Option A: mem.Stack ---
    stack: mem.Stack
    mem.stack_init(&stack, buf)
    stack_alloc := mem.stack_allocator(&stack)

    // --- Option B: mem.Buddy_Allocator ---
	buddy: mem.Buddy_Allocator
	mem.buddy_allocator_init(&buddy, buf, mem.DEFAULT_ALIGNMENT)
	buddy_alloc := mem.buddy_allocator(&buddy)

    // --- Option C: mem.Arena ---
    arena: mem.Arena
    mem.arena_init(&arena, buf)
    arena_alloc := mem.arena_allocator(&arena)

	nums := make([]int, 100, arena_alloc)
	nums[0] = 42

	items := make([dynamic]string, arena_alloc)
	append(&items, "Hello", "Odin")

	fmt.println("First number:", nums[0])
	fmt.println("Dynamic items:", items[:])


    // Use them as normal
    numbers := make([]int, 5, stack_alloc)
    numbers[0] = 42
    fmt.println("Stack allocated:", numbers[0])
}

/*

All three allocators(stack,buddy,arena) in this example are purely carving
sub-allocations out of that same pre-allocated buf.

Calling free_all(arena_alloc) or mem.arena_free_all(&arena) on bump/stack
allocators does not actually return memory to the OS; it only resets the internal
offset/cursor back to the start of buf so you can reuse the buffer. When main() exits,
the defer virtual.releas(...) relases the entire 64 virtual memory block and return to the OS.

When to use them free_all(arena_alloc) or mem.arena_free_all(&arena) if we need to reuse the buffer
in a loop(at the end of frame in game loop or per-request in server) to reset the allocator for the next
pass without having to allocate fresh OS memory.

*/
