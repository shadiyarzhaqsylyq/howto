#include <stdint.h>
#include <assert.h>

typedef uint64_t PageID;
// Pointer swizzling
#define SWIZZLE_MASK    (1ULL << 63)
#define TABLE_ID_MASK   (0x7FFFFFFFULL) // 31 bits max (Bits 0-30)
#define PAGE_NO_MASK    (0xFFFFFFFFULL) // 32 bits max (Bits 0-31)

// 1. GENERATE PageID (Guaranteed Bit 63 = 0)
static inline PageID make_page_id(uint32_t table_id, uint32_t page_no) {
    // Assert table_id fits in 31 bits so it never touches bit 63
    assert((table_id & ~TABLE_ID_MASK) == 0 && "table_id exceeds 31 bits!"); // If program crashes it outputs the string
    
    return (((uint64_t)table_id & TABLE_ID_MASK) << 32) | ((uint64_t)page_no & PAGE_NO_MASK);
}

// 2. EXTRACT TableID
static inline uint32_t get_table_id(PageID id) {
    assert((id & SWIZZLE_MASK) == 0 && "Cannot extract table_id from a swizzled memory pointer!");
    return (uint32_t)((id >> 32) & TABLE_ID_MASK);
}

// 3. EXTRACT PageNO
static inline uint32_t get_page_no(PageID id) {
    assert((id & SWIZZLE_MASK) == 0 && "Cannot extract page_no from a swizzled memory pointer!");
    return (uint32_t)(id & PAGE_NO_MASK);
}
