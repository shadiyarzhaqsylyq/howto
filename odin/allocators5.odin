package main

import "core:fmt"
import "core:mem"
import "core:mem/virtual"

main :: proc() {
    // 1. Reserve 1 GB of virtual address space (pages committed lazily on write)
    v_arena: virtual.Arena
    err := virtual.arena_init_static(&v_arena, 1 * mem.Gigabyte)
    if err != nil {
        panic("Failed to initialize virtual arena")
    }
    defer virtual.arena_destroy(&v_arena)

    v_alloc := virtual.arena_allocator(&v_arena)

    // --- USE CASE 1: Direct allocation via virtual.Arena ---
    // Physical RAM is only committed as this slice touches pages
    direct_data := make([]int, 10_000, v_alloc)
    direct_data[0] = 99

    // --- USE CASE 2: Powering a mem.Stack allocator ---
    // Slice out 2 MB from the virtual arena to back a Stack allocator
    stack_buf := make([]byte, 2 * mem.Megabyte, v_alloc)

    stack: mem.Stack
    mem.stack_init(&stack, stack_buf)
    stack_alloc := mem.stack_allocator(&stack)

    menu_items := make([]string, 3, stack_alloc)
    menu_items[0] = "Start Game"

    fmt.println("Direct value:", direct_data[0])
    fmt.println("Stack string:", menu_items[0])

    // Reset the virtual arena back to 0 bytes used (decommits unused OS pages)
    virtual.arena_free_all(&v_arena)
}
