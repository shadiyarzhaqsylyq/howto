#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// Top bit (63) set to 1 indicates an unswizzled page_id.
// On x86-64/ARM64 Linux, canonical user-space addresses have bit 63 = 0.
#define UNSWIZZLED_TAG (1ULL << 63)

typedef struct Node Node;

// Represents a 64-bit tagged child pointer
typedef struct {
    uint64_t raw;
} SwizzledPtr;

struct Node {
    uint64_t page_id;
    SwizzledPtr child; // Points to child Node or holds child page_id
    int payload;
};

// --- Bit Manipulation Helpers ---

static inline bool is_swizzled(SwizzledPtr ptr) {
    return (ptr.raw & UNSWIZZLED_TAG) == 0;
}

static inline uint64_t get_page_id(SwizzledPtr ptr) {
    return ptr.raw & ~UNSWIZZLED_TAG;
}

static inline Node* get_pointer(SwizzledPtr ptr) {
    return (Node*)ptr.raw;
}

static inline SwizzledPtr make_unswizzled(uint64_t page_id) {
    return (SwizzledPtr){ .raw = page_id | UNSWIZZLED_TAG };
}

static inline SwizzledPtr make_swizzled(Node* ptr) {
    uint64_t addr = (uint64_t)ptr;
    // Verify that the MSB is 0 (valid canonical user pointer)
    assert((addr & UNSWIZZLED_TAG) == 0 && "Pointer MSB must be 0 for swizzling");
    return (SwizzledPtr){ .raw = addr };
}

// --- Simulated Disk & I/O Subsystem ---

static Node* mock_disk_fetch(uint64_t page_id) {
    printf("  [DISK I/O] Reading page_id %lu from disk into RAM frame...\n", page_id);
    
    // Allocate frame memory (in real engine: acquire from pool frame buffer)
    Node* frame = (Node*)malloc(sizeof(Node));
    frame->page_id = page_id;
    frame->child = make_unswizzled(0); // Leaf node or uninitialized child
    frame->payload = (int)(page_id * 100);
    return frame;
}

// --- Core Swizzling Logic ---

// Resolves a child reference. If unswizzled, faults in the page and swizzles the pointer.
Node* resolve_and_swizzle(Node* parent) {
    if (is_swizzled(parent->child)) {
        printf("  [FAST PATH] Pointer is SWIZZLED (Addr: 0x%lx). Traversing in ~2ns.\n", parent->child.raw);
        return get_pointer(parent->child);
    }

    // Slow Path: Unswizzled page fault
    uint64_t child_page_id = get_page_id(parent->child);
    printf("  [SLOW PATH] Pointer is UNSWIZZLED (page_id: %lu). Page fault triggered.\n", child_page_id);

    // 1. Fetch from storage / allocator
    Node* child_node = mock_disk_fetch(child_page_id);

    // 2. Swizzle parent reference (In atomic setup: use atomic CAS / store)
    parent->child = make_swizzled(child_node);
    printf("  [SWIZZLE] Updated parent pointer -> %p\n", (void*)child_node);

    return child_node;
}

// Reverts a swizzled pointer back to a page_id and frees the frame.
void unswizzle_and_evict(Node* parent) {
    if (!is_swizzled(parent->child)) {
        printf("  [EVICT] Node is already unswizzled.\n");
        return;
    }

    Node* child_node = get_pointer(parent->child);
    uint64_t child_page_id = child_node->page_id;

    printf("  [UNSWIZZLE] Converting frame %p back to page_id %lu...\n", (void*)child_node, child_page_id);

    // 1. Flush dirty changes if modified (omitted for brevity)
    printf("  [DISK I/O] Flushing page_id %lu to storage...\n", child_page_id);

    // 2. Atomic pointer update to unswizzled state
    parent->child = make_unswizzled(child_page_id);

    // 3. Free buffer frame back to memory pool
    free(child_node);
    printf("  [EVICT] Memory frame freed.\n");
}

// --- Demonstration ---

int main(void) {
    // 1. Initialize parent node on page 100 pointing to unswizzled child page 200
    Node root = {
        .page_id = 100,
        .child = make_unswizzled(200),
        .payload = 1
    };

    printf("=== FIRST TRAVERSAL (Unswizzled / Page Fault) ===\n");
    Node* child1 = resolve_and_swizzle(&root);
    printf("Child payload: %d\n\n", child1->payload);

    printf("=== SECOND TRAVERSAL (Swizzled / In-Memory Direct Hop) ===\n");
    Node* child2 = resolve_and_swizzle(&root);
    printf("Child payload: %d\n\n", child2->payload);

    printf("=== EVICTION & UNSWIZZLING ===\n");
    unswizzle_and_evict(&root);
    printf("\n");

    printf("=== THIRD TRAVERSAL (Post-Eviction Page Fault) ===\n");
    Node* child3 = resolve_and_swizzle(&root);
    printf("Child payload: %d\n\n", child3->payload);

    // Cleanup final state
    unswizzle_and_evict(&root);

    return 0;
}
