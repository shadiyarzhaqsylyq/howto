package main

import "core:fmt"



main :: proc(){
c := context

context.user_index = 456
{
context.allocator = context.allocator
context.user_index = 123
supertramp()
fmt.println(context.user_index)
}
assert(context.user_index == 456)
fmt.println(context.user_index)

}

supertramp :: proc() {
	c := context // this `context` is the same as the parent procedure that it was called from
	// From this example, context.user_index == 123
	// A context.allocator is assigned to the return value of `my_custom_allocator()`
	fmt.println(context.user_index) //prints 123
	// The memory management procedure uses the `context.allocator` by default unless explicitly specified otherwise
	ptr := new(int)
	free(ptr)
}
