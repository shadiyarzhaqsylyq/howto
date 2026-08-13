#include <stdio.h>
// fopen(), fputs(), fclose()
int main() {
    // Open file in write mode ("w")
    FILE *fp = fopen("output.txt", "w");
    
    // Check if the file opened successfully
    if (fp == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Write strings to the file
    // Note: \n must be added explicitly to break lines
    fputs("Hello, World!\n", fp);
    fputs("Learning C file handling.\n", fp);

    // Close the file to save progress
    fclose(fp);
    
    printf("Data written successfully.\n");
    return 0;
}
