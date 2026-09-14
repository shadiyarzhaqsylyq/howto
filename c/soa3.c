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
    // Allocate all 16 MB in a single contiguous block and 4MB for each block
float *buffer = malloc(4 * NUM_PARTICLES * sizeof(float));
if (!buffer) return 1;

struct ParticleSystem ps;
ps.x  = buffer; // Fisrt 1 million x0,x1,x2...xN are between 0-4MB and last element is at 4MB
ps.y  = buffer + NUM_PARTICLES; // next elements are between 4-8MB. First element starts at 4MB
ps.vx = buffer + (2 * NUM_PARTICLES); // 8-12MB
ps.vy = buffer + (3 * NUM_PARTICLES); // 12-16MB


    
    if (!ps.x || !ps.y || !ps.vx || !ps.vy) {
        fprintf(stderr, "Allocation failed!\n");
        return 1;
    }

    // High-performance batch processing (Compiler auto-vectorizes this easily)
    for (int i = 0; i < NUM_PARTICLES; i++) {
        ps.x[i] += ps.vx[i];
        ps.y[i] += ps.vy[i];
    }
	printf("%p\n",(void*)buffer);
	printf("%p\n",buffer);
	printf("%f\n",*buffer);
	printf("%f\n",buffer[0]);

	printf("%p\n", (void*)buffer);    // 0x7ff8a4000000
	printf("%p\n", (void*)ps.x);      // 0x7ff8a4000000 (starts at the exact same location)
	printf("%p\n", (void*)&ps.x[0]);  // 0x7ff8a4000000
// Clean up everything with a single free call
free(buffer); // free(ps.x) works equally well

    return 0;
}
