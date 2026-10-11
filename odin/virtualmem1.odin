package main

import "core:fmt"
import vmem "core:mem/virtual"

Vertex :: struct {
    x, y, z: f32,
}

main :: proc() {
    count :: 10 // Number of structs you want to store
    byte_size := count * size_of(Vertex)

    // Request anonymous virtual memory from the OS
    data, err := vmem.reserve_and_commit(byte_size)
    if err != .None {
        fmt.println("Failed to allocate virtual memory:", err)
        return
    }
    // Release the memory block when scope exits
    defer vmem.release(raw_data(data), len(data))

    // Reinterpret the raw byte buffer pointer as a slice of your struct type
    vertices := ([^]Vertex)(raw_data(data))[:count]

    // Read and write directly to the array of structs
    vertices[0] = Vertex{1.0, 2.0, 3.0}
    vertices[1] = Vertex{4.0, 5.0, 6.0}

    fmt.println("Vertex 0:", vertices[0])
    fmt.println("Vertex 1:", vertices[1])
}
