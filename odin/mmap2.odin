package main

import "core:fmt"
import "core:mem"
import "core:sys/linux"

PAGE_SIZE :: 16 * mem.Kilobyte // 16 KB page
NUM_PAGES :: 4096              // 64 MB total buffer pool

Buffer_Pool :: struct {
    raw_memory: []byte,
    num_frames: int,
}

// O(1) direct access to any frame's bytes — zero allocator overhead
get_frame :: #force_inline proc(pool: ^Buffer_Pool, frame_id: int) -> []byte {
    assert(frame_id >= 0 && frame_id < pool.num_frames)
    offset := frame_id * PAGE_SIZE
    return pool.raw_memory[offset : offset + PAGE_SIZE]
}

main :: proc() {
    total_size: uint = NUM_PAGES * PAGE_SIZE

    ptr, err := linux.mmap(
        0,
        total_size,
        {.READ, .WRITE},
        {.PRIVATE, .ANONYMOUS},
        -1,
        0,
    )
    if err != nil {
        fmt.eprintln("mmap failed:", err)
        return
    }
    defer linux.munmap(ptr, total_size)

    pool := Buffer_Pool{
        raw_memory = ([^]byte)(ptr)[:int(total_size)],
        num_frames = NUM_PAGES,
    }

    // Access Frame #42 directly
    frame_42 := get_frame(&pool, 42)
    fmt.println("Frame 42 byte length:", len(frame_42)) // 16384 bytes
}
