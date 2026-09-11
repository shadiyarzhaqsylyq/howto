package main

import "core:fmt"
import "core:mem"

Fixed_Key :: struct #packed {
    tenant_id: u32,
    user_id:   u64,
}

main :: proc() {
    // 1. Suppose we have an integer (which takes up 4 bytes)
    value: i32 = 0x12345678 //305_419_896
	fixed_k := Fixed_Key{tenant_id = 1, user_id = 100921}
    
    // 2. Get a raw pointer to it
    ptr := &value 
    
    // 3. Convert that pointer into a 4-byte slice view
    bytes := mem.byte_slice(ptr, size_of(i32))
    bytes1 := mem.byte_slice(&fixed_k, size_of(Fixed_Key))
    // bytes is now of type []byte, with a length of 4
    fmt.println("Slice length:", len(bytes)) // Output: 4
    fmt.printf("Bytes hex: %x\n", bytes)    // Output: [78, 56, 34, 12] (on Little-Endian systems) or %v [120,86,52,18]
	fmt.printf("Bytes hex: %x\n", bytes1)
}
