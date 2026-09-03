package main

import "core:fmt"
import "core:c"

// Link against system OpenSSL libraries
when ODIN_OS == .Windows {
    foreign import libssl { "system:libssl.lib", "system:libcrypto.lib" }
} else {
    foreign import libssl   "system:ssl"
    foreign import libcrypto "system:crypto"
}

// C function bindings
foreign libssl {
    OPENSSL_init_ssl :: proc(opts: u64, settings: rawptr) -> c.int ---
    TLS_method       :: proc() -> rawptr ---
    SSL_CTX_new      :: proc(method: rawptr) -> rawptr ---
    SSL_CTX_free     :: proc(ctx: rawptr) ---
}

foreign libcrypto {
    OpenSSL_version :: proc(type: c.int) -> cstring ---
}

main :: proc() {
    // Print OpenSSL version
    version := OpenSSL_version(0)
    fmt.printf("OpenSSL Version: %s\n", version)

    // Initialize SSL context
    OPENSSL_init_ssl(0, nil)
    method := TLS_method()
    ctx := SSL_CTX_new(method)
    if ctx == nil {
        fmt.println("Failed to create SSL context.")
        return
    }
    defer SSL_CTX_free(ctx)

    fmt.println("Successfully created OpenSSL SSL_CTX!")
}
