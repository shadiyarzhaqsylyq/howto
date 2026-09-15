package main

import "core:fmt"
import "core:mem"

main :: proc() {
    // 1. Backing buffer (stack, static, or virtual memory)
    backing_buffer: [64 * mem.Kilobyte]byte

    // 2. Initialize built-in mem.Arena
    arena: mem.Arena
    mem.arena_init(&arena, backing_buffer[:])

    // 3. Get the standard mem.Allocator interface
    alloc := mem.arena_allocator(&arena)

    // 4. Use it identically to your custom code
    context.allocator = alloc

    list := make([dynamic]int)
    defer delete(list) // or skip delete and call free_all at the end
    
    append(&list, 10, 20, 30)
    fmt.println("List:", list[:])
    fmt.printf("Arena peak used: %d bytes\n", arena.peak_used)

    // 5. Reset the arena back to offset 0
    mem.arena_free_all(&arena)
    // or free_all(alloc)
}
