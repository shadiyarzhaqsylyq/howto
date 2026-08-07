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

	b := Baz(Foo{
		a = 100,
		b = "I am a Baz",
	})



	(^Foo)(&b).a = 200
	(^Foo)(&b).b = "Updated Baz"

	fmt.printfln("Foo variable: %v", f)
	fmt.printfln("Baz variable (casted to Foo to print): %v", Foo(b))
}

