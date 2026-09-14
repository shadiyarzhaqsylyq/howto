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
