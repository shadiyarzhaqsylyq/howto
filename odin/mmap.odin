package main

import "core:fmt"
import "core:mem"
import "core:sys/linux"

main :: proc() {
    // 1. Length for linux.mmap must be of type 'uint'
    size: uint = 64 * mem.Megabyte

    // 2. Pass 0 instead of nil for uintptr
    ptr, err := linux.mmap(
        0,                                   // addr: uintptr
        size,                                // length: uint
        {.READ, .WRITE},                     // prot
        {.PRIVATE, .ANONYMOUS},              // flags
        -1,                                  // fd
        0,                                   // offset
    )

    // 3. linux.Errno compares against nil (or .NONE)
    if err != nil {
        fmt.eprintln("mmap failed:", err)
        return
    }
    defer linux.munmap(ptr, size)

    // 4. Convert pointer to slice (slice indexing requires 'int')
    mapped_slice := ([^]byte)(ptr)[:int(size)]

    // 5. Initialize the arena with your mmap'd memory
    arena: mem.Arena
    mem.arena_init(&arena, mapped_slice)

    allocator := mem.arena_allocator(&arena)

    // 6. Test allocation
    data := make([dynamic]int, allocator)
    append(&data, 100, 200, 300)

    fmt.println("Allocated inside mmap'd arena:", data[:])
}
