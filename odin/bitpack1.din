package main

import "core:fmt"

// Using a 'distinct' type prevents accidentally mixing Page_ID with raw integers
Page_ID :: distinct u64

// 1. GENERATE Page_ID (Bit Packing)

make_page_id :: #force_inline proc(table_id: u32, page_num: u32) -> Page_ID {
    return Page_ID((u64(table_id) << 32) | u64(page_num))
}

// 2. EXTRACT TableID (Shift right by 32 bits)

get_table_id :: #force_inline proc(id: Page_ID) -> u32 {
    return u32(u64(id) >> 32)
}

// 3. EXTRACT PageNO (Truncate to 32 bits)

get_page_num :: #force_inline proc(id: Page_ID) -> u32 {
    return u32(id)
}

// Fast 64-bit bit-mixer (SplitMix64)
// Note: In Odin, '~' is the bitwise XOR operator (not '^')

hash_page_id :: #force_inline proc(page_id: Page_ID) -> u64 {
    z := u64(page_id) + 0x9e37_79b9_7f4a_7c15 // Golden ratio constant
    z = (z ~ (z >> 30)) * 0xbf58_476d_1ce4_e5b9
    z = (z ~ (z >> 27)) * 0x94d0_49bb_1331_11eb
    return z ~ (z >> 31)
}

// Map hash to bucket index (Power of Two)
get_bucket_index :: #force_inline proc(page_id: Page_ID, num_buckets: uint) -> uint {
    assert(num_buckets > 0 && (num_buckets & (num_buckets - 1)) == 0, "num_buckets must be a power of 2")
    return uint(hash_page_id(page_id)) & (num_buckets - 1)
}

main :: proc() {
    table_id: u32 = 1
    page_number: u32 = 1

    // Create PageID
    page_id := make_page_id(table_id, page_number)

    table := get_table_id(page_id)
    pagenum := get_page_num(page_id)

    // Example bucket lookup
    num_buckets: uint = 2
    bucket_index := get_bucket_index(page_id, num_buckets)

    fmt.printfln("PageID       = %v", page_id)
    fmt.printfln("TableID      = %v", table)
    fmt.printfln("Page Number  = %v", pagenum)
    fmt.printfln("Bucket Index = %v", bucket_index)
}
