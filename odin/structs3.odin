package main

import "core:fmt"

Foo :: struct { a: i32, b: string }

main :: proc() {
    f := Foo{a = 42, b = "Hello"}

    // Store a pointer to 'f' inside an 'any' variable
    val: any = &f

    // VALID: Assert that 'val' holds a ^Foo pointer and access field 'a'
    val.(^Foo).a = 100

    fmt.printfln("Updated Foo: %v", f) // Output: Foo{a = 100, b = "Hello"}
}
