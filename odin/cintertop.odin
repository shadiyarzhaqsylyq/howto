package main

import "core:fmt"
import "core:c"
//OpenSSL
foreign import libcrypto { "system:ssl", "system:crypto" }

foreign libcrypto {
    // Built-in 'cstring' used without the 'c.' prefix
    OpenSSL_version :: proc(type: c.int) -> cstring ---
    SHA256          :: proc(d: [^]u8, n: c.size_t, md: [^]u8) -> [^]u8 ---
}

main :: proc() {
    version := OpenSSL_version(0)
    fmt.printfln("OpenSSL Version: %s", string(version))

    input := "Hello, Odin!"
    hash: [32]u8

    SHA256(raw_data(input), len(input), raw_data(hash[:]))
    fmt.printfln("SHA-256: %x", hash)
}
