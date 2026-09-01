package main

import "core:fmt"
import "core:mem"


my_custom_alloc :: mem.nil_allocator

main :: proc(){
c := context

context.user_index = 456
{
context.allocator = my_custom_alloc() // or mem.nil_allocator(), get_current_alloc()
context.user_index = 123
supertramp()
fmt.println(context.user_index)
}
assert(context.user_index == 456)
fmt.println(context.user_index)

}

supertramp :: proc() {
	c := context // 
	// context.user_index == 123
	// uses my_custom_alloc()
	fmt.println(context.user_index) // prints 123
	// The memory management procedure uses the `context.allocator` by default unless explicitly specified otherwise
	ptr := new(int)
	free(ptr)
}

/*
get_current_alloc :: proc() -> mem.Allocator {
    return context.allocator
}


*/
