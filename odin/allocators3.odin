package main

import "core:fmt"

main :: proc() {
    // Uses Odin's built-in thread-local scratch arena
    context.allocator = context.temp_allocator

    list := make([dynamic]int)
    append(&list, 10, 20, 30)
    
    fmt.println("List:", list[:])

    // Free all temporary allocations made in this scope/frame
    free_all(context.temp_allocator)
}
