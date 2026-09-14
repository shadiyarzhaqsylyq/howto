#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
// Pointer Swizzling
// MSB (Bit 63) is used as the unswizzled tag.
// On x86_64, user-space virtual addresses always have Bit 63 set to 0.
#define UNSWIZZLED_TAG ((uint64_t)1 << 63)

typedef struct {
    uint32_t page_id;
    char data[64]; // Sample page payload
} Page;

// Tagging Bit-Masking Helpers
static inline bool is_swizzled(uint64_t ref) {
    return (ref & UNSWIZZLED_TAG) == 0; // Bit 63 is 0 -> Swizzled (Memory Pointer)
}

static inline uint64_t make_unswizzled_ref(uint32_t page_id) {
    return UNSWIZZLED_TAG | ((uint64_t)page_id); // Bit 63 is 1 -> Unswizzled (PageID)
}

static inline uint32_t extract_page_id(uint64_t ref) {
    return (uint32_t)(ref & ~UNSWIZZLED_TAG);
}

// Simulated Buffer Pool / Disk I/O Manager
Page* buffer_pool_fetch_page(uint32_t page_id) {
    printf("  [PAGE FAULT] PageID %u not in memory. Fetching from disk into buffer pool...\n", page_id);
    Page* page = (Page*)malloc(sizeof(Page));
    page->page_id = page_id;
    snprintf(page->data, sizeof(page->data), "Data payload of Page #%u", page_id);
    return page;
}

// Core Swizzling Dereference Function
Page* resolve_page_ref(uint64_t* ref_slot) {
    uint64_t ref = *ref_slot;

    // --- FAST PATH ---
    // Zero hash table lookup, zero locks. Just a single bit check.
    if (is_swizzled(ref)) {
        printf("  [FAST PATH] Swizzled pointer detected. Direct memory access!\n");
        return (Page*)ref;
    }

    // --- SLOW PATH (Page Fault & Swizzle) ---
    uint32_t page_id = extract_page_id(ref);
    Page* page_ptr = buffer_pool_fetch_page(page_id);

    // Swizzle in-place: Rewrite parent slot with virtual memory pointer
    *ref_slot = (uint64_t)page_ptr;
    printf("  [SWIZZLE] In-place update: Replaced PageID %u with pointer %p\n", page_id, (void*)page_ptr);

    return page_ptr;
}

int main(void) {
    // A parent B-Tree node child pointer initialized to PageID 1042 (Unswizzled)
    uint64_t child_page_ref = make_unswizzled_ref(1042);

    printf("Initial child_page_ref value: 0x%016llX (Tag bit set, PageID = 1042)\n\n", 
           (unsigned long long)child_page_ref);

    // --- 1st Access: Page Fault & Swizzling ---
    printf("--- First Access ---\n");
    Page* page = resolve_page_ref(&child_page_ref);
    printf("Read Payload: \"%s\"\n", page->data);
    printf("Updated child_page_ref value: 0x%016llX (Tag bit cleared, Raw Pointer)\n\n", 
           (unsigned long long)child_page_ref);

    // --- 2nd Access: Fast Path Direct Dereference ---
    printf("--- Second Access ---\n");
    page = resolve_page_ref(&child_page_ref);
    printf("Read Payload: \"%s\"\n\n", page->data);

    // Clean up
    free(page);
    return 0;
}
