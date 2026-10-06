package main

import "core:fmt"
import "core:mem"
import "core:mem/virtual"

Table_Slot :: struct {
    page_id:  u64,
    frame_id: u32,
    psl:      u8,
}

init_page_table :: proc(slot_count: int) -> []Table_Slot {
    bytes := slot_count * size_of(Table_Slot)
    mem, err := virtual.reserve_and_commit(bytes)
    if err != nil do panic("Failed to allocate page table")
    return ([^]Table_Slot)(raw_data(mem))[:slot_count]
}


main :: proc() {
    // 1. Allocate backing buffer directly from the OS (no heap/malloc overhead)
    size := uint(16 * mem.Megabyte)
    buf, err := virtual.reserve_and_commit(size)
    if err != nil {
        panic("Failed to reserve virtual memory")
    }
    defer virtual.release(raw_data(buf), size)

    // PHASE 1: Loading (Arena)
    {
        arena: mem.Arena
        mem.arena_init(&arena, buf)
        alloc := mem.arena_allocator(&arena)

        data := make([]int, 1000, alloc)
        data[0] = 123

        free_all(alloc)
    }

    // PHASE 2: Gameplay (Buddy)
    {
        buddy: mem.Buddy_Allocator
        mem.buddy_allocator_init(&buddy, buf, mem.DEFAULT_ALIGNMENT)
        alloc := mem.buddy_allocator(&buddy)

        entity_a := new(int, alloc)
        free(entity_a, alloc)

        free_all(alloc)
    // without calling free() there will be not enough memory for other objects. If we need allocation for many
    // objects, we need to use free().
    // We can use free_all() at the end, if we dont care about memory, there are not many objects allocations.
    }

    // PHASE 3: Menu (Stack)
    {
        stack: mem.Stack
        mem.stack_init(&stack, buf)
        alloc := mem.stack_allocator(&stack)

        menu := make([]string, 3, alloc)
        menu[0] = "Start Game"

        free_all(alloc)
    }
}
