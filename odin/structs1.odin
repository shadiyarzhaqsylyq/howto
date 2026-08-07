package main

import "core:fmt"

Foo :: struct {
	a: i32,
	b: string,
}

Baz :: distinct Foo

main :: proc() {
	f := Foo{
		a = 42,
		b = "I am a Foo",
	}

	// Clean initialization of distinct struct
	b := Baz{
		a = 100,
		b = "I am a Baz",
	}

	// Clean field modification
	b.a = 200
	b.b = "Updated Baz"

	fmt.printfln("Foo variable: %v", f)
	fmt.printfln("Baz variable: %v", b) // fmt handles distinct types automatically
}
