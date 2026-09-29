#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t PageID;
typedef uint32_t FrameID;

#define INVALID_PAGE_ID  (~(uint64_t)0)
#define INVALID_FRAME_ID (~(uint32_t)0)

// 1. Bit-packing functions
static inline PageID make_page_id(uint32_t table_id, uint32_t page_num) {
    return ((uint64_t)table_id << 32) | (uint64_t)page_num;
}
static inline uint32_t get_table_id(PageID id) { return (uint32_t)(id >> 32); }
static inline uint32_t get_page_num(PageID id) { return (uint32_t)id; }

// 2. High-performance 64-bit mixer (SplitMix64)
static inline uint64_t hash_page_id(PageID key) {
    key ^= key >> 30;
    key *= 0xbf58476d1ce4e5b9ULL;
    key ^= key >> 27;
    key *= 0x94d049bb133111ebULL;
    key ^= key >> 31;
    return key;
}

// 3. Slot structure (16 bytes per entry: exactly 4 slots per 64-byte cache line)
typedef struct {
    PageID   page_id;   // 8 bytes (INVALID_PAGE_ID if empty)
    FrameID  frame_id;  // 4 bytes
    uint32_t dib;       // 4 bytes: Distance from Initial Bucket
} HashSlot;

typedef struct {
    HashSlot *slots;
    size_t    capacity; // Must be a power of 2
    size_t    mask;     // capacity - 1
    size_t    count;
} PageTable;

PageTable* page_table_create(size_t capacity) {
    // Ensure capacity is a power of 2
    PageTable *pt = (PageTable*)malloc(sizeof(PageTable));
    pt->capacity = capacity;
    pt->mask = capacity - 1;
    pt->count = 0;
    pt->slots = (HashSlot*)malloc(capacity * sizeof(HashSlot));

    for (size_t i = 0; i < capacity; i++) {
        pt->slots[i].page_id = INVALID_PAGE_ID;
        pt->slots[i].frame_id = INVALID_FRAME_ID;
        pt->slots[i].dib = 0;
    }
    return pt;
}

void page_table_destroy(PageTable *pt) {
    free(pt->slots);
    free(pt);
}

// 4. LOOKUP: O(1) average, with early-exit optimization
bool page_table_lookup(const PageTable *pt, PageID page_id, FrameID *out_frame_id) {
    size_t idx = hash_page_id(page_id) & pt->mask;
    uint32_t cur_dib = 0;

    while (true) {
        const HashSlot *slot = &pt->slots[idx];

        // Slot empty -> Key definitely does not exist
        if (slot->page_id == INVALID_PAGE_ID) {
            return false;
        }

        // Key found!
        if (slot->page_id == page_id) {
            *out_frame_id = slot->frame_id;
            return true;
        }

        // Robin Hood early exit: If the slot's DIB is smaller than our current DIB,
        // the key cannot exist further down the line.
        if (slot->dib < cur_dib) {
            return false;
        }

        cur_dib++;
        idx = (idx + 1) & pt->mask;
    }
}

// 5. INSERT: "Take from the rich, give to the poor"
bool page_table_insert(PageTable *pt, PageID page_id, FrameID frame_id) {
    if (pt->count >= pt->capacity) return false; // Table full

    HashSlot incoming = {
        .page_id = page_id,
        .frame_id = frame_id,
        .dib = 0
    };

    size_t idx = hash_page_id(incoming.page_id) & pt->mask;

    while (true) {
        HashSlot *slot = &pt->slots[idx];

        // 1. Found an empty slot: place here and finish
        if (slot->page_id == INVALID_PAGE_ID) {
            *slot = incoming;
            pt->count++;
            return true;
        }

        // 2. Key already exists: update frame mapping
        if (slot->page_id == incoming.page_id) {
            slot->frame_id = incoming.frame_id;
            return true;
        }

        // 3. Robin Hood swap: The incoming item has traveled further than 
        // the existing item (it is "poorer"), so it steals the slot.
        if (incoming.dib > slot->dib) {
            HashSlot temp = *slot;
            *slot = incoming;
            incoming = temp; // Continue inserting the displaced ("richer") item
        }

        incoming.dib++;
        idx = (idx + 1) & pt->mask;
    }
}

// 6. DELETE: Backward-shift deletion (avoids tombstones)
bool page_table_delete(PageTable *pt, PageID page_id) {
    size_t idx = hash_page_id(page_id) & pt->mask;
    uint32_t cur_dib = 0;

    // Find the item
    while (true) {
        HashSlot *slot = &pt->slots[idx];
        if (slot->page_id == INVALID_PAGE_ID || slot->dib < cur_dib) {
            return false; // Not found
        }
        if (slot->page_id == page_id) {
            break; // Found at 'idx'
        }
        cur_dib++;
        idx = (idx + 1) & pt->mask;
    }

    // Backward shift subsequent items to fill the hole
    while (true) {
        size_t next_idx = (idx + 1) & pt->mask;
        HashSlot *next_slot = &pt->slots[next_idx];

        // Stop if next slot is empty or already in its ideal bucket (dib == 0)
        if (next_slot->page_id == INVALID_PAGE_ID || next_slot->dib == 0) {
            pt->slots[idx].page_id = INVALID_PAGE_ID;
            pt->slots[idx].frame_id = INVALID_FRAME_ID;
            pt->slots[idx].dib = 0;
            break;
        }

        // Shift item backward
        pt->slots[idx] = *next_slot;
        pt->slots[idx].dib--; // Traveled one slot less now
        idx = next_idx;
    }

    pt->count--;
    return true;
}


int main() {
    // Sized for a small buffer pool: 64 slots (power of 2)
    PageTable *pt = page_table_create(64);

    PageID p1 = make_page_id(1, 100);
    PageID p2 = make_page_id(1, 101);
    PageID p3 = make_page_id(2, 100);

    // Insert mappings: PageID -> FrameID
    page_table_insert(pt, p1, 0); // maps to buffer frame 0
    page_table_insert(pt, p2, 1); // maps to buffer frame 1
    page_table_insert(pt, p3, 2); // maps to buffer frame 2

    // Lookups
    FrameID frame;
    if (page_table_lookup(pt, p2, &frame)) {
        printf("Found Page (table=%u, page=%u) at Frame %u\n",
               get_table_id(p2), get_page_num(p2), frame);
    }

    // Delete an evicted page
    page_table_delete(pt, p2);

    if (!page_table_lookup(pt, p2, &frame)) {
        printf("Page p2 successfully evicted / removed.\n");
    }

    page_table_destroy(pt);
    return 0;
}
