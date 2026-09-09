package main

import "core:fmt"
import "core:mem"
import "core:mem/virtual"

main :: proc() {
    // -------------------------------------------------------------
    // Step 1: Ask OS for memory using core:mem/virtual
    // -------------------------------------------------------------
    raw_buffer, err := virtual.reserve_and_commit(64 * mem.Megabyte)
    if err != nil {
        panic("OS out of memory")
    }
    defer virtual.release(raw_data(raw_buffer), len(raw_buffer))

    // -------------------------------------------------------------
    // Step 2: Manage that memory with an allocator from core:mem
    // -------------------------------------------------------------
    arena: mem.Arena
    mem.arena_init(&arena, raw_buffer)
    arena_allocator := mem.arena_allocator(&arena)

    // -------------------------------------------------------------
    // Step 3: Use make() and new() with that custom allocator!
    // -------------------------------------------------------------
    // Slice allocated inside the arena:
    numbers := make([]int, 100, arena_allocator)
    numbers[0] = 42

    // Dynamic array allocated inside the arena:
    items := make([dynamic]string, arena_allocator)
    append(&items, "Hello", "Odin")

    fmt.println("First number:", numbers[0])
    fmt.println("Dynamic items:", items[:])

    // -------------------------------------------------------------
    // Step 4: Bulk reset everything when done
    // -------------------------------------------------------------
    mem.arena_free_all(&arena)
}
