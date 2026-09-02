package main

import "core:c"
import "core:fmt"
// BearSSL example
// Link to the compiled static library
when ODIN_OS == .Linux {
    foreign import bearssl "libbearssl.a"
} else when ODIN_OS == .Windows {
    foreign import bearssl "bearssl.lib"
}

// Replicate the BearSSL C struct structures
br_sha256_context :: struct {
    vtable: rawptr,
    buf:    [64]c.uint8_t,
    val:    [8]c.uint32_t,
    count:  c.uint64_t,
}

// Bind to the exact C function names using the "c" calling convention
@(default_calling_convention="c")
foreign bearssl {
    @(link_name="br_sha256_init")
    sha256_init :: proc(ctx: ^br_sha256_context) ---
}

main :: proc() {
    ctx: br_sha256_context
    
    // Call BearSSL directly
    sha256_init(&ctx)
    
    fmt.println("BearSSL SHA-256 context successfully initialized!")
}

/*
OpenSSL

package main

import "core:fmt"
import "core:c"

foreign import libcrypto "system:crypto"

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



*/
