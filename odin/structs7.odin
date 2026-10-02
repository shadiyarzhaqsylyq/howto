package main

import "core:fmt"

Page_Header :: struct #packed {
    page_lsn:     u64,  // Log Sequence Number for write-ahead logging (WAL)
    page_id:      u32,
    next_page:    u32,
    prev_page:    u32,
    item_count:   u16,
    free_top:     u16,  // Grows downward (offset from start of page)
    free_bottom:  u16,  // Grows upward (offset from start of page)
    flags:        u16,
}

Page_Header2 :: struct #packed {
    page_lsn:     u64,  // 8 bytes
    page_id:      u32,  // 4 bytes
    next_page:    u32,  // 4 bytes
    prev_page:    u32,  // 4 bytes
    item_count:   u16,  // 2 bytes
    free_top:     u16,  // 2 bytes
    free_bottom:  u16,  // 2 bytes
    flags:        u16,  // 2 bytes
    reserved:     u32,  // 4 bytes <-- Added to pad exactly to 32 bytes, automatically zero-initialized to 0 by default
}


main :: proc() {
    // Get size by passing the type
    struct_size := size_of(Page_Header)
    fmt.println("Size of Page_Header:", struct_size) // Outputs: 28 

    struct_size2 := size_of(Page_Header2)
    fmt.println("Size of Page_Header:", struct_size2) // Outputs: 32




}


