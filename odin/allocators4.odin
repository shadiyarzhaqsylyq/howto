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

   
    stack: mem.Stack
    mem.stack_init(&stack, buf)
    stack_alloc := mem.stack_allocator(&stack)

    
	buddy: mem.Buddy_Allocator
	mem.buddy_allocator_init(&buddy, buf, mem.DEFAULT_ALIGNMENT)
	buddy_alloc := mem.buddy_allocator(&buddy)

    
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

