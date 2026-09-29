#include <stdint.h>


typedef struct {
    unsigned int lp_off   : 15; // Bits 0..14  (Offset)
    unsigned int lp_flags : 2;  // Bits 15..16 (Slot flags/status)
    unsigned int lp_len   : 15; // Bits 17..31 (Tuple length)
} ItemIdData; // sizeof(ItemIdData) is exactly 4 bytes

typedef struct {
    uint32_t lp_off   : 15;
    uint32_t lp_flags : 2;
    uint32_t lp_len   : 15;
} ItemIdData; // Explicitly tied to 32-bit integer allocation

uint16_t offset = 8120; // Fits in 15 bits (0 to 32,767)
uint8_t  flags  = 1;    // Fits in 2 bits  (0 to 3)
uint16_t length = 72;   // Fits in 15 bits (0 to 32,767)

// Combine into one uint32_t (4 bytes):
uint32_t slot = ((uint32_t)offset & 0x7FFF) 
              | (((uint32_t)flags & 0x03) << 15) 
              | (((uint32_t)length & 0x7FFF) << 17);


uint16_t extracted_offset =  slot & 0x7FFF;         // Read lower 15 bits
uint8_t  extracted_flags  = (slot >> 15) & 0x03;    // Shift right 15, read 2 bits
uint16_t extracted_length = (slot >> 17) & 0x7FFF;  // Shift right 17, read 15 bits
