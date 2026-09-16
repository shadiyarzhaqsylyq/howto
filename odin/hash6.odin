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
hash_mixed_with_string1 :: proc(tenant_id: u64, dept: string, seed: u64 = 0) -> u64 {
	h_str := xxhash.XXH64(transmute([]u8)dept, seed)
	h_int := splitmix64(tenant_id)
	return hash_combine(h_int, h_str)
}

hash_mixed_with_string2 :: proc(key: ^Short_Composite_Key, dept: string, seed: u64 = 0) -> u64 {
    // 1. Hash the string once with XXH64
    h_str := xxhash.XXH64(transmute([]u8)dept, seed)

    // 2. Mix the two fixed fields without losing any bits
    h_tenant := splitmix64(u64(key.order_id))
    h_user   := splitmix64(key.user_id)
    h_fixed  := hash_combine(h_tenant, h_user)

    // 3. Combine fixed part + string part
    return hash_combine(h_fixed, h_str)
}



// 2. Short Fixed Key (<= 16B)
Short_Composite_Key :: struct {
	order_id: u64,
	user_id:  u64,
}

hash_short_composite :: proc(key: ^Short_Composite_Key) -> u64 {
	h1 := splitmix64(key.order_id)
	h2 := splitmix64(key.user_id)
	return hash_combine(h1, h2)
}
//Alternative
hash_short_composite :: #force_inline proc(key: ^Short_Composite_Key) -> u64 {
	return hash_combine(key.order_id, key.user_id)
}


// 3. Wide Fixed Key (>= 24B)
Wide_Composite_Key :: struct #packed {
	tenant_id:   u64,
	account_id:  u64,
	region_id:   u64,
	status_flag: u8,
}

hash_wide_composite :: proc(key: ^Wide_Composite_Key, seed: u64 = 0) -> u64 {
	bytes := mem.byte_slice(key, size_of(Wide_Composite_Key))
	return xxhash.XXH64(bytes, seed)
}

main :: proc() {
	wide_key := Wide_Composite_Key{
		tenant_id   = 1,
		account_id  = 5555,
		region_id   = 99,
		status_flag = 1,
	}
	h1 := hash_wide_composite(&wide_key)
	fmt.printf("[Technique 3 - Wide >=24B]  Hash: 0x%016X\n", h1)





    CAPACITY: u64 = 1000

	

    // 1. Single int join
    h2 := splitmix64(100921)
    bucket1 := get_bucket_index(h2, CAPACITY)

    // 2. Composite fixed join
    short_key := Short_Composite_Key{order_id = 1001, user_id = 9999}
    h3 := hash_short_composite(&short_key)
    bucket2 := get_bucket_index(h3, CAPACITY)

    // 3. Mixed key join (fixed + string)
    h4 := hash_mixed_with_string1(1002, "engineering")
    bucket3 := get_bucket_index(h4, CAPACITY)

    fmt.printfln("Single Int Bucket:      %d", bucket1)
    fmt.printfln("Composite Fixed Bucket: %d", bucket2)
    fmt.printfln("Mixed String Bucket:    %d", bucket3)



	key1 := Short_Composite_Key{order_id = 1001, user_id = 9999}
    dept1 := "engineering"

    // Row 2 from Table B (exact match)
    key2 := Short_Composite_Key{order_id = 1001, user_id = 9999}
    dept2 := "engineering"

    // Row 3 from Table C (different department)
    key3 := Short_Composite_Key{order_id = 1001, user_id = 9999}
    dept3 := "marketing"

    h5 := hash_mixed_with_string2(&key1, dept1)//h4,h5,h6
    h6 := hash_mixed_with_string2(&key2, dept2)
    h7 := hash_mixed_with_string2(&key3, dept3)

    fmt.printfln("Row 1 Hash: 0x%016x", h5)
    fmt.printfln("Row 2 Hash: 0x%016x (Matches Row 1: %v)", h6, h5 == h6)
    fmt.printfln("Row 3 Hash: 0x%016x (Matches Row 1: %v)", h7, h5 == h7)



}
