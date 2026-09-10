package main

import "core:fmt"
import "core:mem"

main :: proc() {
    // 1. Allocate backing buffer ONCE
    buf, _ := make_aligned([]byte, 16 * mem.Megabyte, mem.DEFAULT_ALIGNMENT)
    defer delete(buf)

    // ========================================================
    // PHASE 1: Loading Phase using an ARENA
    // Fast, linear allocations, no individual frees needed.
    // ========================================================
    {
        arena: mem.Arena
        mem.arena_init(&arena, buf)
        alloc := mem.arena_allocator(&arena)

        // allocate lots of loading stuff...
        data := make([]int, 1000, alloc)
        data[0] = 123

        // Done with loading phase:
        free_all(alloc) 
    }

    // ========================================================
    // PHASE 2: Gameplay Phase using a BUDDY ALLOCATOR
    // We reuse 'buf'. A Buddy Allocator allows random-order 
    // allocations and frees (like malloc/free) without leaking.
    // ========================================================
    {
        buddy: mem.Buddy_Allocator
        mem.buddy_allocator_init(&buddy, buf, mem.DEFAULT_ALIGNMENT)
        alloc := mem.buddy_allocator(&buddy)

        // Spawn dynamic entities
        entity_a := new(int, alloc)
        entity_b := new(int, alloc)

        // Buddy allows freeing in ANY order:
        free(entity_a, alloc) // Entity A died, free it!
        
        // Entity B is still valid:
        entity_b^ = 456
        fmt.println("Entity B:", entity_b^)

        free(entity_b, alloc)

        // Reset the buddy allocator when gameplay ends:
        free_all(alloc)
    // without calling free() there will be not enough memory.
    }

    // ========================================================
    // PHASE 3: Menu / UI Phase using a STACK ALLOCATOR
    // We reuse 'buf' AGAIN. Stack allocators allow LIFO 
    // (Last-In, First-Out) popping and push/pop scopes.
    // ========================================================
    {
        stack: mem.Stack
        mem.stack_init(&stack, buf)
        alloc := mem.stack_allocator(&stack)

        menu_items := make([]string, 3, alloc)
        menu_items[0] = "Start Game"

        // Done with Menu phase:
        free_all(alloc)
    }
}
