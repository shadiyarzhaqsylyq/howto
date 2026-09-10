package main

import "core:fmt"
import "core:mem"

Person :: struct {
    name: string,
    age:  int,
}

main :: proc() {
    // 1. Allocate a backing buffer (e.g., 1 MB on the heap or stack)
    backing_buffer := make([]byte, 1 * mem.Megabyte) //or buf, err := make_aligned([]byte, 1 * mem.Megabyte, mem.DEFAULT_ALIGNMENT)
    defer delete(backing_buffer)

    // 2. Initialize the arena with the backing buffer
    arena: mem.Arena
    mem.arena_init(&arena, backing_buffer)

    // 3. Obtain an Allocator interface from the arena
    arena_allocator := mem.arena_allocator(&arena)

    // 4. Allocate memory using the arena allocator
    p := new(Person, arena_allocator)
    p.name = "Alice"
    p.age = 30

    numbers := make([]int, 5, arena_allocator)
    for i in 0..<5 {
        numbers[i] = (i + 1) * 10
    }

    fmt.println("Person:", p^)
    fmt.println("Numbers:", numbers)

    // 5. Free / reset everything in the arena at once
    free_all(arena_allocator) // or: mem.arena_free_all(&arena)
}
