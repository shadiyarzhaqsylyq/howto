#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

typedef uint64_t PageID;

// 1. GENERATE PageID (Bit Packing)
static inline PageID make_page_id(uint32_t table_id, uint32_t page_num) {
    return ((uint64_t)table_id << 32) | (uint64_t)page_num;
}

// 2. EXTRACT TableID (Shift right by 32 bits)
static inline uint32_t get_table_id(PageID id) {
    return (uint32_t)(id >> 32);
}

// 3. EXTRACT PageNO (Mask out the high 32 bits)
static inline uint32_t get_page_num(PageID id) {
    return (uint32_t)id; // or (id & 0xFFFFFFFFULL);
}



// Fast 64-bit bit-mixer (SplitMix64)
static inline uint64_t hash_page_id(uint64_t page_id) {
    uint64_t z = page_id + 0x9e3779b97f4a7c15ULL; // Golden ratio constant
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// Example usage in the Page Table
// Power of Two
size_t get_bucket_index(uint64_t page_id, size_t num_buckets) {
    // If num_buckets is a power of 2:
    return hash_page_id(page_id) & (num_buckets - 1);
}

int main(void) {
    uint32_t tableid = 1;
    uint32_t pagenumber = 2;

    // Declare variable instance of type PageID
    PageID page_id = make_page_id(tableid, pagenumber);

    uint32_t table = get_table_id(page_id);
    uint32_t pagenum = get_page_num(page_id);

    printf("PageID = %llu\n", page_id); // or printf("PageID = llu%\n", (unsigned long long)page_id) for uint64_t
    printf("TableID = %u\n", table); //or printf("PageID = u%\n", (unsigned int)page_id) for uint32_t
    printf("Page Number = %u\n", (unsigned int)pagenum);
	// printf("%d\n", (int)page_id) for int32_t
	//  printf("%lld\n", (long long)page_id) for int64_t

    return 0;
}
