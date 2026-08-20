package main

import "core:fmt"
import "core:hash/xxhash"
import "core:mem"

main :: proc() {
    // 1. In Odin, strings can be cast or passed directly as a byte slice []u8
    input_text := "Hello"
    data := transmute([]u8)input_text
    
    // 2. Compute the 32-bit xxHash integer (using seed 0)
    hash_value: u32 = xxhash.XXH32(data, 0)
    fmt.printf("Numerical Hash: %v\n", hash_value) // Output: 4060533391

    // 3. Extract the raw byte array (Little-Endian)
    // We cast the u32 pointer directly into a 4-byte array pointer
    byte_array_ptr := cast(^[4]u8)&hash_value
    byte_array := byte_array_ptr^

    fmt.printf("Little-Endian Byte Array: %v\n", byte_array) 
    // Output: [143, 210, 6, 242]
    
    // 4. Access individual bytes just like you asked
    fmt.printf("First byte: %v, Last byte: %v\n", byte_array[0], byte_array[3])
    // Output: First byte: 143, Last byte: 242
}
