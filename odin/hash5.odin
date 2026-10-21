package main

import "core:fmt"
import "core:mem"
import "core:hash/xxhash"

// Daniel Lemire (Fastrange)
//Prefer this for Grace/Hybrid Hash Join
get_bucket_index :: proc(hash: u64, capacity: u64) -> u64 {
    product := u128(hash) * u128(capacity)
    return u64(product >> 64)
}
// Power of Two
get_bucket_index_power_of_2 :: proc(hash: u64, capacity: u64) -> u64 {
    return hash & (capacity - 1)
}


// JOIN ON a.id = b.id
splitmix64 :: #force_inline proc(x: u64) -> u64 {
	z := x + 0x9E3779B97F4A7C15
	z = (z ~ (z >> 30)) * 0xBF58476D1CE4E5B9
	z = (z ~ (z >> 27)) * 0x94D049BB133111EB
	return z ~ (z >> 31)
}

hash_combine :: #force_inline proc(h1, h2: u64) -> u64 {
	return splitmix64(h1 ~ (h2 + 0x9E3779B97F4A7C15 + (h1 << 6) + (h1 >> 2)))
}

// 1. Mixed Key
// 2 columns
// JOIN ON a.tenant_id = b.tenant_id AND a.dept = b.dept
hash_mixed_with_string :: proc(tenant_id: u64, dept: string, seed: u64 = 0) -> u64 {
	h_str := xxhash.XXH64(transmute([]u8)dept, seed)
	h_int := splitmix64(tenant_id)
	return hash_combine(h_int, h_str)
}





main :: proc() {
    CAPACITY: u64 = 16

	

    // 1. Single int join
    h1 := splitmix64(100921)
    bucket1 := get_bucket_index(h1, CAPACITY)

    // 3. Mixed key join (fixed + string)
    h2 := hash_mixed_with_string(1002, "engineering")
    bucket2 := get_bucket_index(h2, CAPACITY)

    fmt.printfln("Single Int Bucket:      %d", bucket1)
    fmt.printfln("Mixed String Bucket:    %d", bucket2)



	
    dept1 := "engineering"
    dept2 := "engineering"
    dept3 := "marketing"

    h3 := hash_mixed_with_string(1001, dept1)//h4,h5,h6
    h4 := hash_mixed_with_string(1001, dept2)
    h7 := hash_mixed_with_string(1001, dept3)

    fmt.printfln("Row 1 Hash: 0x%016x", h3)
    fmt.printfln("Row 2 Hash: 0x%016x (Matches Row 1: %v)", h4, h3 == h4)
    fmt.printfln("Row 3 Hash: 0x%016x (Matches Row 1: %v)", h5, h3 == h5)



}
