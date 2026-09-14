#include <stdio.stdio.h>
#include <stdio.h>

#define NUM_PARTICLES 4

// Struct of Arrays layout
struct ParticleSystem {
    float x[NUM_PARTICLES];
    float y[NUM_PARTICLES];
    float vx[NUM_PARTICLES];
    float vy[NUM_PARTICLES];
};

int main(void) {
    struct ParticleSystem ps;

    // 1. Initialize attributes independently
    for (int i = 0; i < NUM_PARTICLES; i++) {
        ps.x[i]  = (float)i * 10.0f;
        ps.y[i]  = 0.0f;
        ps.vx[i] = 1.5f;
        ps.vy[i] = 0.5f;
    }

    // 2. Batch update (Operations touch contiguous memory for one attribute)
    for (int i = 0; i < NUM_PARTICLES; i++) {
        ps.x[i] += ps.vx[i];
        ps.y[i] += ps.vy[i];
    }

    // Print updated positions
    for (int i = 0; i < NUM_PARTICLES; i++) {
        printf("Particle %d position: (%.1f, %.1f)\n", i, ps.x[i], ps.y[i]);
    }

    return 0;
}
