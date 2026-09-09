package main

import "core:fmt"
import "core:mem"
import "core:mem/virtual"

main :: proc() {
    // 1. Get raw virtual memory from the OS (no heap/malloc involved)
    size := uint(64 * mem.Megabyte)
    buf, err := virtual.reserve_and_commit(size)
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

    // Use them as normal
    numbers := make([]int, 5, stack_alloc)
    numbers[0] = 42
    fmt.println("Stack allocated:", numbers[0])
}
