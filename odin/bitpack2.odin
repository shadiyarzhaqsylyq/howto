package main

import "core:fmt"

// Define the bit field backed by a 32-bit unsigned integer (exactly 4 bytes)
ItemIdData :: bit_field u32 {
    lp_off:   u16 | 15, // Bits 0..14  (Offset)
    lp_flags: u8  | 2,  // Bits 15..16 (Slot flags)
    lp_len:   u16 | 15, // Bits 17..31 (Tuple length)
}
// With key
ItemIdData :: bit_field u64 {
    lp_off:   u16 | 15, // Bits 0..14  (Offset inside page)
    lp_flags: u8  | 2,  // Bits 15..16 (Status)
    lp_len:   u16 | 15, // Bits 17..31 (Length)
    key_num:  u32 | 32, // Bits 32..63 (32-bit Key Number)
} // sizeof(ItemIdData) is now 8 bytes

main :: proc {
    // 1. Initialize bitfield
    item := ItemIdData{
        lp_off   = 8120,
        lp_flags = 1,
        lp_len   = 72,
    }

    // 2. Read values directly
    fmt.println("Offset:", item.lp_off)     // 8120
    fmt.println("Flags:",  item.lp_flags)   // 1
    fmt.println("Length:", item.lp_len)     // 72

    // 3. Convert bitfield to raw u32 (and back)
    raw_slot := transmute(u32)item
    fmt.printf("Raw slot u32: 0x%X\n", raw_slot)

    restored_item := transmute(ItemIdData)raw_slot
    fmt.println("Restored offset:", restored_item.lp_off)

        offset: u16 = 8120
    flags:  u8  = 1
    length: u16 = 72

    // Combine into one u32 (4 bytes)
    slot: u32 = u32(offset & 0x7FFF) |
                (u32(flags & 0x03) << 15) |
                (u32(length & 0x7FFF) << 17)

    // Extract fields back out
    extracted_offset := u16(slot & 0x7FFF)         // Read lower 15 bits
    extracted_flags  := u8((slot >> 15) & 0x03)    // Shift right 15, read 2 bits
    extracted_length := u16((slot >> 17) & 0x7FFF)  // Shift right 17, read 15 bits

    fmt.println("Extracted Offset:", extracted_offset)
    fmt.println("Extracted Flags:",  extracted_flags)
    fmt.println("Extracted Length:", extracted_length)
}



