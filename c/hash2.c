#include <stdio.h>
#include <stdint.h>
#define XXH_INLINE_ALL 
#include "xxhash.h"

int main(void) {
    // Dynamically allocate the state to guarantee the strict 64-byte alignment
    XXH3_state_t* state = XXH3_createState();
    if (state == NULL) {
        return 1;
    }

    // Reset the state for a new 64-bit XXH3 streaming session
    XXH3_64bits_reset(state);

    uint32_t tenant_id = 42;
    uint64_t user_id   = 100921;

    // 1. Hash first column (instead of transmute, pass the address and size)
    XXH3_64bits_update(state, &tenant_id, sizeof(tenant_id));

    // 2. Hash second column into the same state
    XXH3_64bits_update(state, &user_id, sizeof(user_id));

    // 3. Finalize digest
    XXH64_hash_t composite_hash = XXH3_64bits_digest(state);
    printf("Composite Hash: 0x%016llx\n", (unsigned long long)composite_hash);

    // Free the aligned state memory
    XXH3_freeState(state);

    return 0;
}
