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
