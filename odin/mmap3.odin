// 1
package main

import "core:fmt"
import "core:mem"
import "core:sys/linux"

Player :: struct {
    id:    int,
    score: f32,
}

main :: proc() {
    size: uint = 64 * mem.Megabyte
    ptr, err := linux.mmap(0, size, {.READ, .WRITE}, {.PRIVATE, .ANONYMOUS}, -1, 0)
    if err != nil {
        fmt.eprintln("mmap failed")
        return
    }
    defer linux.munmap(ptr, size)

    mapped_slice := (cast([^]byte)ptr)[:size]

    // Initialize the arena
    arena: mem.Arena
    mem.arena_init(&arena, mapped_slice)
    allocator := mem.arena_allocator(&arena)

    // 1. Create a dynamic array of structs using the arena
    players := make([dynamic]Player, allocator)
    
    append(&players, Player{id = 101, score = 95.5})
    append(&players, Player{id = 102, score = 88.0})

    // 2. Allocate a single struct on the arena heap
    special_player := new(Player, allocator)
    special_player^ = Player{id = 999, score = 999.9}

    fmt.println("Dynamic Players:", players[:])
    fmt.println("Special Player:", special_player^)
}

// 2
package main

import "core:fmt"
import "core:sys/linux"
import "core:mem"

Vector3 :: struct {
    x, y, z: f32,
}

main :: proc() {
    size: uint = 64 * mem.Megabyte
    ptr, err := linux.mmap(0, size, {.READ, .WRITE}, {.PRIVATE, .ANONYMOUS}, -1, 0)
    if err != nil do return
    defer linux.munmap(ptr, size)

    // Calculate how many Vector3 structs can fit in your mmap block
    count := size / size_of(Vector3)

    // Cast raw pointer to a multi-pointer of Vector3 and slice it
    vectors := (cast([^]Vector3)ptr)[:count]

    // Write directly to the mmap memory without any allocator overhead
    vectors[0] = Vector3{1.0, 2.0, 3.0}
    vectors[1] = Vector3{4.0, 5.0, 6.0}

    fmt.println("Vector 0:", vectors[0])
    fmt.println("Vector 1:", vectors[1])
}

// 3
package main

import "core:fmt"
import "core:sys/linux"
import "core:mem"

Entity :: struct {
    id:    u32,
    x, y:  f32,
    active: bool,
}

main :: proc() {
    // Allocate 64 MB of virtual memory
    size: uint = 64 * mem.Megabyte
    ptr, err := linux.mmap(0, size, {.READ, .WRITE}, {.PRIVATE, .ANONYMOUS}, -1, 0)
    if err != nil do return
    defer linux.munmap(ptr, size)

    // We want to store 10,000 structs
    count :: 10_000
    
    // Verify our mmap block is large enough
    assert(size >= count * size_of(Entity))

    // Cast pointer to a slice of 10,000 entities
    entities := (cast([^]Entity)ptr)[:count]

    // Populate them with data
    for i in 0..<count {
        entities[i] = Entity{
            id     = u32(i),
            x      = f32(i) * 1.5,
            y      = f32(i) * 2.0,
            active = true,
        }
    }

    // Access them effortlessly
    fmt.println("Entity #0:", entities[0])
    fmt.println("Entity #5000:", entities[5000])
    fmt.println("Entity #9999:", entities[9999])
}
