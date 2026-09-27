#include <stdio.h>
#include <stdlib.h>

void fill_array(int *arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = i * 10; // Modifying heap data directly
    }
}

int main() {
    int size = 5;
    // 1. Allocate memory on the heap
    int *my_ptr = malloc(size * sizeof(int));
    
    if (my_ptr == NULL) return 1; // Always check for failure

    // 2. Pass the pointer to the function
    fill_array(my_ptr, size);

    printf("First element: %d\n", my_ptr[0]); // Outputs 0

    // 3. Clean up
    free(my_ptr);
    return 0;
}
