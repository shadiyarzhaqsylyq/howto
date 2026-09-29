#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t PageID;
typedef uint32_t FrameID;

#define INVALID_PAGE_ID  (~(uint64_t)0)
#define INVALID_FRAME_ID (~(uint32_t)0)
#define MAX_LOAD_FACTOR  0.75

// Bit-packing utilities
static inline PageID make_page_id(uint32_t table_id, uint32_t page_num) {
    return ((uint64_t)table_id << 32) | (uint64_t)page_num;
}
static inline uint32_t get_table_id(PageID id) { return (uint32_t)(id >> 32); }
static inline uint32_t get_page_num(PageID id) { return (uint32_t)id; }

// Rounds up to the nearest power of 2
static inline size_t next_pow2(size_t n) {
    if (n <= 16) return 16;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

// 64-bit SplitMix64 Hash Mixer
static inline uint64_t hash_page_id(PageID key) {
    key ^= key >> 30;
    key *= 0xbf58476d1ce4e5b9ULL;
    key ^= key >> 27;
    key *= 0x94d049bb133111ebULL;
    key ^= key >> 31;
    return key;
}

// 16-byte slot: 4 slots align perfectly to 64-byte CPU cache lines
typedef struct {
    PageID   page_id;   // 8 bytes
    FrameID  frame_id;  // 4 bytes
    uint32_t dib;       // 4 bytes: Distance from Initial Bucket
} HashSlot;

typedef struct {
    HashSlot *slots;
    size_t    capacity; // Always power of 2
    size_t    mask;     // capacity - 1
    size_t    count;
} PageTable;

// Forward declaration for dynamic resizing
static bool page_table_resize(PageTable *pt, size_t new_capacity);

// Helper for allocating 64-byte cache-aligned slot arrays
static HashSlot* allocate_slots(size_t capacity) {
    HashSlot *slots = NULL;
#if defined(_MSC_VER) || defined(__MINGW32__)
    slots = (HashSlot*)_aligned_malloc(capacity * sizeof(HashSlot), 64);
#else
    if (posix_memalign((void**)&slots, 64, capacity * sizeof(HashSlot)) != 0) {
        return NULL;
    }
#endif
    if (!slots) return NULL;

    for (size_t i = 0; i < capacity; i++) {
        slots[i].page_id = INVALID_PAGE_ID;
        slots[i].frame_id = INVALID_FRAME_ID;
        slots[i].dib = 0;
    }
    return slots;
}

static void free_slots(HashSlot *slots) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(slots);
#else
    free(slots);
#endif
}

PageTable* page_table_create(size_t initial_capacity) {
    PageTable *pt = (PageTable*)malloc(sizeof(PageTable));
    if (!pt) return NULL;

    pt->capacity = next_pow2(initial_capacity);
    pt->mask = pt->capacity - 1;
    pt->count = 0;
    pt->slots = allocate_slots(pt->capacity);

    if (!pt->slots) {
        free(pt);
        return NULL;
    }

    return pt;
}

void page_table_destroy(PageTable *pt) {
    if (!pt) return;
    free_slots(pt->slots);
    free(pt);
}

// LOOKUP: O(1) average, with early-exit on smaller DIB
bool page_table_lookup(const PageTable *pt, PageID page_id, FrameID *out_frame_id) {
    size_t idx = hash_page_id(page_id) & pt->mask;
    uint32_t cur_dib = 0;

    while (true) {
        const HashSlot *slot = &pt->slots[idx];

        if (slot->page_id == INVALID_PAGE_ID || slot->dib < cur_dib) {
            return false; // Not found or passed insertion point
        }

        if (slot->page_id == page_id) {
            if (out_frame_id) *out_frame_id = slot->frame_id;
            return true;
        }

        cur_dib++;
        idx = (idx + 1) & pt->mask;
    }
}

// Internal raw insertion helper (without load factor check)
static void page_table_insert_raw(HashSlot *slots, size_t mask, HashSlot incoming) {
    size_t idx = hash_page_id(incoming.page_id) & mask;

    while (true) {
        HashSlot *slot = &slots[idx];

        if (slot->page_id == INVALID_PAGE_ID) {
            *slot = incoming;
            return;
        }

        if (slot->page_id == incoming.page_id) {
            slot->frame_id = incoming.frame_id;
            return;
        }

        // Steal from the rich
        if (incoming.dib > slot->dib) {
            HashSlot temp = *slot;
            *slot = incoming;
            incoming = temp;
        }

        incoming.dib++;
        idx = (idx + 1) & mask;
    }
}

// Automatically double table capacity when load factor threshold is met
static bool page_table_resize(PageTable *pt, size_t new_capacity) {
    HashSlot *new_slots = allocate_slots(new_capacity);
    if (!new_slots) return false;

    size_t new_mask = new_capacity - 1;

    // Rehash and migrate existing slots
    for (size_t i = 0; i < pt->capacity; i++) {
        if (pt->slots[i].page_id != INVALID_PAGE_ID) {
            HashSlot slot = pt->slots[i];
            slot.dib = 0; // Reset DIB for new hash placement
            page_table_insert_raw(new_slots, new_mask, slot);
        }
    }

    free_slots(pt->slots);
    pt->slots = new_slots;
    pt->capacity = new_capacity;
    pt->mask = new_mask;

    return true;
}

// INSERT / UPDATE
bool page_table_insert(PageTable *pt, PageID page_id, FrameID frame_id) {
    // 1. If key already exists, update in-place without triggering resize
    FrameID existing_frame;
    if (page_table_lookup(pt, page_id, &existing_frame)) {
        size_t idx = hash_page_id(page_id) & pt->mask;
        while (pt->slots[idx].page_id != page_id) {
            idx = (idx + 1) & pt->mask;
        }
        pt->slots[idx].frame_id = frame_id;
        return true;
    }

    // 2. Check load factor threshold (grow table before inserting if needed)
    if ((double)(pt->count + 1) / (double)pt->capacity > MAX_LOAD_FACTOR) {
        if (!page_table_resize(pt, pt->capacity * 2)) {
            return false; // Re-allocation failed
        }
    }

    // 3. Insert new item
    HashSlot incoming = {
        .page_id = page_id,
        .frame_id = frame_id,
        .dib = 0
    };

    page_table_insert_raw(pt->slots, pt->mask, incoming);
    pt->count++;
    return true;
}

// DELETE: Backward-shift deletion (avoids tombstones)
bool page_table_delete(PageTable *pt, PageID page_id) {
    size_t idx = hash_page_id(page_id) & pt->mask;
    uint32_t cur_dib = 0;

    // Search for key
    while (true) {
        HashSlot *slot = &pt->slots[idx];
        if (slot->page_id == INVALID_PAGE_ID || slot->dib < cur_dib) {
            return false; // Key not present
        }
        if (slot->page_id == page_id) {
            break;
        }
        cur_dib++;
        idx = (idx + 1) & pt->mask;
    }

    // Backward shift subsequent items to fill the gap
    while (true) {
        size_t next_idx = (idx + 1) & pt->mask;
        HashSlot *next_slot = &pt->slots[next_idx];

        if (next_slot->page_id == INVALID_PAGE_ID || next_slot->dib == 0) {
            pt->slots[idx].page_id = INVALID_PAGE_ID;
            pt->slots[idx].frame_id = INVALID_FRAME_ID;
            pt->slots[idx].dib = 0;
            break;
        }

        pt->slots[idx] = *next_slot;
        pt->slots[idx].dib--;
        idx = next_idx;
    }

    pt->count--;
    return true;
}

int main() {
    PageTable *pt = page_table_create(16);

    // Insert elements to trigger auto-resizing
    for (uint32_t i = 0; i < 50; i++) {
        PageID pid = make_page_id(1, i);
        page_table_insert(pt, pid, i * 10);
    }

    printf("Table count: %zu, capacity: %zu\n", pt->count, pt->capacity);

    FrameID frame;
    PageID target = make_page_id(1, 25);
    if (page_table_lookup(pt, target, &frame)) {
        printf("Page 25 found at Frame %u\n", frame);
    }

    // Delete item
    page_table_delete(pt, target);
    if (!page_table_lookup(pt, target, NULL)) {
        printf("Page 25 successfully deleted.\n");
    }

    page_table_destroy(pt);
    return 0;
}
