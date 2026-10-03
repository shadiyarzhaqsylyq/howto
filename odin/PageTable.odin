package buffer_pool

import "core:sync"

PAGE_SIZE :: 8192

Page_ID  :: distinct u32
Frame_ID :: distinct u32

// Packed 12-byte or 16-byte struct (32 KiB per 2048-slot partition)
Page_Table_Slot :: struct {
    page_id:  Page_ID,
    frame_id: Frame_ID,
    occupied: bool,
}

Partition :: struct {
    lock:     sync.RWMutex,
    slots:    []Page_Table_Slot, // Separate make() slice per partition
    capacity: u32,
    count:    u32,
}

Partitioned_Page_Table :: struct {
    partitions: []Partition, // Array of partitions
    mask:       u32,         // For fast bitwise modulo if total_partitions is power of 2
}

// Fast 32-bit bit-mixer (Murmur3 finalizer) for uniform hash distribution
hash_page_id :: proc(page_id: Page_ID) -> u32 {
    x := u32(page_id)
    x ^= x >> 16
    x *= 0x85ebca6b
    x ^= x >> 13
    x *= 0xc2b2ae35
    x ^= x >> 16
    return x
}

init_page_table :: proc(pt: ^Partitioned_Page_Table, num_partitions: u32, slots_per_partition: u32 = 2048) {
    pt.partitions = make([]Partition, num_partitions)
    pt.mask = num_partitions - 1 // Assumes num_partitions is a power of 2 (e.g., 16, 32, 64)

    for i in 0 ..< num_partitions {
        pt.partitions[i].capacity = slots_per_partition
        // Separate make() call for each partition slice
        pt.partitions[i].slots = make([]Page_Table_Slot, slots_per_partition)
    }
}

// Concurrent lookup
lookup_frame :: proc(pt: ^Partitioned_Page_Table, page_id: Page_ID) -> (Frame_ID, bool) {
    h := hash_page_id(page_id)
    
    // Select partition using top bits or bitmask
    part_idx := h & pt.mask
    part := &pt.partitions[part_idx]

    // Lock ONLY this partition
    sync.shared_lock(&part.lock)
    defer sync.shared_unlock(&part.lock)

    // Probe within partition's local 2048-slot array
    cap := part.capacity
    local_idx := h % cap

    for i: u32 = 0; i < cap; i += 1 {
        slot_idx := (local_idx + i) % cap
        slot := &part.slots[slot_idx]

        if !slot.occupied do break
        if slot.page_id == page_id {
            return slot.frame_id, true
        }
    }

    return 0, false
}

//Alternative
init_page_table :: proc(pt: ^Partitioned_Page_Table, num_partitions: u32 = 1024, slots_per_partition: u32 = 2048) {
    // 1. Allocate the partitions structure array (1st make)
    pt.partitions = make([]Partition, num_partitions)
    pt.mask = num_partitions - 1

    // 2. Allocate the entire 24 MiB table at once (2nd make)
    total_slots := num_partitions * slots_per_partition
    all_slots := make([]Page_Table_Slot, total_slots)

    // 3. Sub-slice the giant array into the partitions
    for i in 0 ..< num_partitions {
        pt.partitions[i].capacity = slots_per_partition
        
        start := i * slots_per_partition
        end   := start + slots_per_partition
        
        // This creates a safe, fast window view into the flat block. 
        // No extra allocations are made here.
        pt.partitions[i].slots = all_slots[start:end]
    }
}

destroy_page_table :: proc(pt: ^Partitioned_Page_Table) {
    if len(pt.partitions) == 0 do return

    // Extract the original backing pointer from the first partition's slice
    // and delete the entire 24 MiB chunk at once.
    all_slots_ptr := raw_data(pt.partitions[0].slots)
    delete(all_slots_ptr) 

    // Delete the partition array
    delete(pt.partitions)
}
