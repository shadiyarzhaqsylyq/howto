package main

import "core:fmt"
import "core:mem"




main :: proc(){
c := context

context.user_index = 456
{
context.allocator = get_current_alloc()
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


get_current_alloc :: proc() -> mem.Allocator {
    return context.allocator
}

/*
work_zero_alloc :: proc() {
    // Override context allocator for this procedure scope
	// Hot Paths & Real Time code: Placing this at the top of render loop, physics tick, or audio processing callback
	guarantees that no code path accidentally hits the heap or causes hidden allocation overhead
    context.allocator = mem.nil_allocator()

    // This implicit allocation attempt will fail:
    p, err := new(int)
    fmt.println(p, err) // Outputs: nil Out_Of_Memory

    // Explicit allocations using another allocator still work:
    // p_temp, _ := new(int, context.temp_allocator) 
}



*/
