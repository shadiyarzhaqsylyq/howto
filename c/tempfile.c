#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    // The template must end with exactly six 'X' characters
    char template[] = "/tmp/secure_file_XXXXXX";
    
    // mkstemp modifies 'template' with random characters and opens it securely
    int fd = mkstemp(template);
    
    if (fd == -1) {
        perror("Failed to create secure temporary file");
        return 1;
    }

    // Print the randomly generated filename
    printf("Secure file created at: %s\n", template);

    // Write data securely using the file descriptor
    write(fd, "Secret Data\n", 12);

    // Clean up
    close(fd);
    unlink(template); // Deletes the file when you are done
    return 0;
}
