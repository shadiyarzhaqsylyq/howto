package main

import "core:fmt"
import "core:hash/xxhash"

main :: proc() {
    key: string = "table_row_primary_key_42"


data := transmute([]u8)key

    // Correct: 2 parameters (input slice, optional named seed)
    h1 := xxhash.XXH64(data, seed = 0)
    h2 := xxhash.XXH64(data, seed = 1337)

    fmt.println("h1:", h1)
    fmt.println("h2:", h2)
}
