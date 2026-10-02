package main

import "core:fmt"
import "core:mem"
import "core:sys/linux"

PAGE_SIZE :: 16 * mem.Kilobyte // 16 KB page
NUM_PAGES :: 4096              // 64 MB total buffer pool

// 1. Define your Page struct to be EXACTLY 16 KB
Page :: struct #align(4096) {
    page_id:    u64,
    lsn:        u64,
    num_rows:   u32,
    is_dirty:   bool,
    // The rest of the 16KB holds raw page data:
    payload:    [PAGE_SIZE - (8 + 8 + 4 + 1)]byte,
}

// Compile-time check: ensures the struct matches PAGE_SIZE exactly
#assert(size_of(Page) == PAGE_SIZE)

Buffer_Pool :: struct {
    // Treat the mmap'd memory directly as an array of Page structs
    pages:      [^]Page,
    num_frames: int,
}

// Returns a direct pointer to the struct inside mmap memory
get_page :: #force_inline proc(pool: ^Buffer_Pool, frame_id: int) -> ^Page {
    assert(frame_id >= 0 && frame_id < pool.num_frames)
    return &pool.pages[frame_id]
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

    // Cast the raw pointer DIRECTLY to a typed array of Page structs:
    pool := Buffer_Pool{
        pages      = ([^]Page)(ptr),
        num_frames = NUM_PAGES,
    }

    // 2. Read and Write directly as a struct!
    p := get_page(&pool, 42)

    p.page_id  = 1001
    p.lsn      = 45892
    p.num_rows = 15
    p.is_dirty = true
    p.payload[0] = 0xAA

    // Proof: read it back
    fmt.println("Page ID:", p.page_id)
    fmt.println("LSN:", p.lsn)
    fmt.println("Num Rows:", p.num_rows)
    fmt.printf("First byte of payload: 0x%X\n", p.payload[0])
}
