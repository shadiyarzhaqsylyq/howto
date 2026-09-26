#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define PAGE_SIZE 4096
#define FANOUT 4
#define BUFFER_POOL_SIZE 4    // Small pool to force eviction
#define TOTAL_DISK_PAGES 16

// Bit 63 is the Swizzle Tag
#define SWIZZLE_MASK (1ULL << 63)

typedef uint64_t swizzle_ptr_t;
typedef uint64_t page_id_t;

typedef struct Frame Frame;

// In-memory B-Tree Node layout inside a 4KB page
typedef struct {
    page_id_t self_page_id;
    int is_leaf;
    int num_keys;
    int64_t keys[FANOUT - 1];
    swizzle_ptr_t children[FANOUT]; // Contains either Page ID or Swizzled Pointer
} Page;

// Buffer Pool Frame metadata
struct Frame {
    Page page;                  // Actual page memory (4KB)
    page_id_t page_id;          // Currently loaded page ID
    bool is_dirty;
    bool is_used;
    bool ref_bit;               // For CLOCK eviction
    Frame *parent_frame;        // Needed for unswizzling upon eviction
    int child_slot_in_parent;   // Which slot in parent points to this frame
};

typedef struct {
    Frame frames[BUFFER_POOL_SIZE];
    size_t clock_hand;
} BufferPool;

// Simulated Disk (Block Device)
Page mock_disk[TOTAL_DISK_PAGES];

/* ==================== Pointer Swizzling Helpers ==================== */

static inline bool is_swizzled(swizzle_ptr_t ptr) {
    return (ptr & SWIZZLE_MASK) != 0;
}

static inline swizzle_ptr_t swizzle(void *frame_ptr) {
    // Tag the 64-bit memory address with bit 63 = 1
    return ((uintptr_t)frame_ptr) | SWIZZLE_MASK;
}

static inline Frame* unmask(swizzle_ptr_t ptr) {
    // Strip tag bit 63 to recover the raw C pointer
    return (Frame*)((uintptr_t)(ptr & ~SWIZZLE_MASK));
}

/* ==================== Simulated Disk I/O ==================== */

void disk_read(page_id_t pid, Page *dest) {
    printf("   [DISK I/O] Read page %lu into RAM\n", pid);
    memcpy(dest, &mock_disk[pid], sizeof(Page));
}

void disk_write(page_id_t pid, const Page *src) {
    printf("   [DISK I/O] Flush dirty page %lu to disk\n", pid);
    memcpy(&mock_disk[pid], src, sizeof(Page));
}

/* ==================== Buffer Manager ==================== */

void bp_init(BufferPool *bp) {
    memset(bp, 0, sizeof(BufferPool));
}

// Unswizzle a child pointer in its parent frame back to a Disk Page ID
void unswizzle_frame(Frame *frame) {
    if (frame->parent_frame != NULL) {
        printf("   [UNSWIZZLE] Parent (Page %lu) slot [%d] reverted from %p to Page ID %lu\n",
               frame->parent_frame->page_id, frame->child_slot_in_parent,
               (void*)frame, frame->page_id);

        // Replace direct memory pointer with raw disk page ID (bit 63 = 0)
        frame->parent_frame->page.children[frame->child_slot_in_parent] = frame->page_id;
        frame->parent_frame->is_dirty = true;
    }
}

// Choose victim using CLOCK replacement algorithm
Frame* bp_evict(BufferPool *bp) {
    while (true) {
        Frame *f = &bp->frames[bp->clock_hand];
        bp->clock_hand = (bp->clock_hand + 1) % BUFFER_POOL_SIZE;

        if (!f->ref_bit) {
            // Evict this frame
            printf(" [EVICT] Evicting Page %lu from Frame %p\n", f->page_id, (void*)f);

            // 1. Unswizzle in parent so parent no longer holds stale memory pointer
            unswizzle_frame(f);

            // 2. Flush if dirty
            if (f->is_dirty) {
                disk_write(f->page_id, &f->page);
                f->is_dirty = false;
            }

            f->is_used = false;
            f->parent_frame = NULL;
            return f;
        }
        f->ref_bit = false;
    }
}

// Find a free frame or evict one
Frame* bp_alloc_frame(BufferPool *bp) {
    for (size_t i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!bp->frames[i].is_used) {
            bp->frames[i].is_used = true;
            return &bp->frames[i];
        }
    }
    Frame *victim = bp_evict(bp);
    victim->is_used = true;
    return victim;
}

/**
 * FIX PAGE (Algorithm 2 from paper):
 * Traverses from a parent to a child slot.
 * - If swizzled: zero hash lookup, instant return!
 * - If unswizzled: page miss -> load from disk -> swizzle parent's pointer.
 */
Frame* fix_child(BufferPool *bp, Frame *parent_frame, int slot) {
    swizzle_ptr_t child_ptr = parent_frame->page.children[slot];

    // --- FAST PATH: PAGE HIT (SWIZZLED) ---
    if (is_swizzled(child_ptr)) {
        Frame *child_frame = unmask(child_ptr);
        child_frame->ref_bit = true;
        printf("-> [HIT] Swizzled dereference to Page %lu at Frame %p (0 Hash Table Lookups!)\n", 
               child_frame->page_id, (void*)child_frame);
        return child_frame;
    }

    // --- SLOW PATH: PAGE MISS (UNSWIZZLED) ---
    page_id_t pid = (page_id_t)child_ptr;
    printf("-> [MISS] Page %lu is on disk. Allocating frame...\n", pid);

    Frame *frame = bp_alloc_frame(bp);
    frame->page_id = pid;
    frame->ref_bit = true;
    frame->is_dirty = false;
    frame->parent_frame = parent_frame;
    frame->child_slot_in_parent = slot;

    // Fetch from disk into frame memory
    disk_read(pid, &frame->page);

    // SWIZZLE: Replace child pointer in parent with memory address
    parent_frame->page.children[slot] = swizzle(frame);
    parent_frame->is_dirty = true;

    printf("   [SWIZZLE] Parent (Page %lu) slot [%d] swizzled to pointer %p\n",
           parent_frame->page_id, slot, (void*)frame);

    return frame;
}

/* ==================== Demonstration / Driver ==================== */

int main(void) {
    BufferPool bp;
    bp_init(&bp);

    // 1. Setup mock data on disk:
    // Root (Page 0) -> Child 0 (Page 1), Child 1 (Page 2), Child 2 (Page 3)
    mock_disk[0].self_page_id = 0;
    mock_disk[0].num_keys = 2;
    mock_disk[0].keys[0] = 100;
    mock_disk[0].keys[1] = 200;
    mock_disk[0].children[0] = 1; // Unswizzled disk Page IDs
    mock_disk[0].children[1] = 2;
    mock_disk[0].children[2] = 3;

    mock_disk[1].self_page_id = 1; mock_disk[1].is_leaf = 1;
    mock_disk[2].self_page_id = 2; mock_disk[2].is_leaf = 1;
    mock_disk[3].self_page_id = 3; mock_disk[3].is_leaf = 1;
    mock_disk[4].self_page_id = 4; mock_disk[4].is_leaf = 1;

    // 2. Load Root (Page 0) permanently into Frame 0
    Frame *root_frame = &bp.frames[0];
    root_frame->is_used = true;
    root_frame->page_id = 0;
    root_frame->parent_frame = NULL;
    disk_read(0, &root_frame->page);

    printf("\n=== RUN 1: Access Child 0 (Cold - Miss) ===\n");
    Frame *c0 = fix_child(&bp, root_frame, 0);

    printf("\n=== RUN 2: Access Child 0 Again (Hot - Pointer Swizzled) ===\n");
    // Notice: Instantaneous dereference. No hash table bucket search!
    c0 = fix_child(&bp, root_frame, 0);

    printf("\n=== RUN 3: Access Child 1 (Cold - Miss) ===\n");
    fix_child(&bp, root_frame, 1);

    printf("\n=== RUN 4: Access Child 2 (Cold - Miss) ===\n");
    fix_child(&bp, root_frame, 2);

    printf("\n=== RUN 5: Pool is Full. Access Child 0 (already swizzled in RAM) ===\n");
    c0 = fix_child(&bp, root_frame, 0);

    printf("\n=== RUN 6: Access Page 4 (Forces Eviction & Unswizzling) ===\n");
    // Root points slot 3 to disk Page 4
    root_frame->page.children[3] = 4;
    fix_child(&bp, root_frame, 3);

    printf("\n=== Demonstration Completed Successfully ===\n");
    return 0;
}
