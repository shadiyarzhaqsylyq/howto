#include <stdio.h>
#include <stdlib.h>

#define NUM_PARTICLES 1000000

// Struct holds heap pointers instead of fixed arrays
struct ParticleSystem {
    float *x;
    float *y;
    float *vx;
    float *vy;
};

int main(void) {
    struct ParticleSystem ps;

    // Allocate 4 MB per array on the Heap (16 MB total)
    ps.x  = malloc(NUM_PARTICLES * sizeof(float));
    ps.y  = malloc(NUM_PARTICLES * sizeof(float));
    ps.vx = malloc(NUM_PARTICLES * sizeof(float));
    ps.vy = malloc(NUM_PARTICLES * sizeof(float));
    /*Alternative
    
    // Allocate all 16 MB in a single contiguous block
float *buffer = malloc(4 * NUM_PARTICLES * sizeof(float));
if (!buffer) return 1;

struct ParticleSystem ps;
ps.x  = buffer;
ps.y  = buffer + NUM_PARTICLES;
ps.vx = buffer + (2 * NUM_PARTICLES);
ps.vy = buffer + (3 * NUM_PARTICLES);

// ... perform computations ...

// Clean up everything with a single free call
free(buffer); // free(ps.x) works equally well
    
    */

    if (!ps.x || !ps.y || !ps.vx || !ps.vy) {
        fprintf(stderr, "Allocation failed!\n");
        return 1;
    }

    // High-performance batch processing (Compiler auto-vectorizes this easily)
    for (int i = 0; i < NUM_PARTICLES; i++) {
        ps.x[i] += ps.vx[i];
        ps.y[i] += ps.vy[i];
    }

    // Clean up heap memory
    free(ps.x);
    free(ps.y);
    free(ps.vx);
    free(ps.vy);

    return 0;
}
