package main

import "core:fmt"
import "core:mem/virtual"

Stack :: struct($T: typeid) {
    data:      [^]T,
    capacity:  int,
    length:    int,
}

init_stack :: proc(s: ^Stack($T), max_items: int) -> bool {
    // virtual.reserve_and_commit requires the size to be a `uint`
    bytes_needed := uint(max_items * size_of(T))
    
    // This correctly acts as mmap(MAP_PRIVATE | MAP_ANONYMOUS) on Posix 
    // and VirtualAlloc(MEM_RESERVE | MEM_COMMIT) on Windows.
    // It returns (data: []byte, err: mem.Allocator_Error)
    data, err := virtual.reserve_and_commit(bytes_needed)
    
    // Now that `err` is properly typed as `mem.Allocator_Error`, `.None` works perfectly
    if err != .None {
        return false
    }

    // raw_data(data) gets the ^byte pointer from the slice, which we cast to our multi-pointer
    s.data = cast([^]T) raw_data(data)
    s.capacity = max_items
    s.length = 0
    
    return true
}

push :: proc(s: ^Stack($T), item: T) {
    s.data[s.length] = item
    s.length += 1
}

pop :: proc(s: ^Stack($T)) -> T {
    s.length -= 1
    return s.data[s.length]
}

destroy_stack :: proc(s: ^Stack($T)) {
    bytes_needed := uint(s.capacity * size_of(T))
    
    // virtual.release takes (data: rawptr, size: uint)
    virtual.release(s.data, bytes_needed)
}

main :: proc() {
    my_stack: Stack(int)
    
    // Reserve space for 1 million integers.
    // Because it's virtual memory, physical RAM is only consumed as we push to it.
    if !init_stack(&my_stack, 1_000_000) {
        fmt.println("Failed to mmap stack memory")
        return
    }
    defer destroy_stack(&my_stack)

    push(&my_stack, 42)
    push(&my_stack, 99)

    fmt.println("Popped:", pop(&my_stack)) // Prints: 99
    fmt.println("Popped:", pop(&my_stack)) // Prints: 42
}
