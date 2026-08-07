

package main

import "core:fmt"

Foo :: struct { a: i32, b: string }
Bar :: struct { x: f32 }

// Union that holds pointers
PtrUnion :: union {
    ^Foo,
    ^Bar,
}

main :: proc() {
    f := Foo{a = 10, b = "Foo"}
    b := Bar{x = 20}


    u: PtrUnion = &f

    // VALID: Assert the union holds a ^Foo pointer and modify field 'a'
    u.(^Foo).a = 99

    u = &b

    u.(^Bar).x = 100

    fmt.printfln("Updated Foo: %v", f)
    fmt.printfln("Updated Foo: %v", b)
}




