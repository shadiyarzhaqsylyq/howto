// Fast runtime representation (unpacked)
struct Row {
    int64_t id;    // 8 bytes
    char flags;    // 1 byte (+ 7 bytes padding)
    double value;  // 8 bytes
};

// Manually copying to a page buffer eliminates padding cleanly
void serialize(const Row& row, char* page_offset) {
    std::memcpy(page_offset, &row.id, 8);
    std::memcpy(page_offset + 8, &row.flags, 1);
    std::memcpy(page_offset + 9, &row.value, 8); // No disk padding!
}


#pragma pack(push, 1)
struct PackedDiskPageHeader {
    int64_t lsn;         // 8 bytes - aligned at 0
    int32_t page_id;     // 4 bytes - aligned at 8
    int16_t slot_count;  // 2 bytes - aligned at 12
    char padding[2];     // 2 bytes - MANUALLY added to bring total to 16
}; // Total size: 16 bytes. Perfectly safe for both disk and CPU.
#pragma pack(pop)


// GCC and Clang approach
struct __attribute__((__packed__)) PackedGcc {
    char a;
    int b; 
}; // Size is 5 bytes

// MSVC (Visual Studio) approach
#pragma pack(push, 1)
struct PackedMsvc {
    char a;
    int b;
}; // Size is 5 bytes
#pragma pack(pop)
