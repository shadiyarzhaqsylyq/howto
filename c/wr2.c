#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>

#define PAGE_SIZE 4096

int main() {
    // 1. Simulate two non-contiguous pages in the Buffer Pool
    char *page_a = malloc(PAGE_SIZE);
    char *page_b = malloc(PAGE_SIZE);
    
    if (!page_a || !page_b) {
        perror("Memory allocation failed");
        return 1;
    }

    // Fill pages with dummy database data
    memset(page_a, 'A', PAGE_SIZE); // Page A filled with 'A's
    memset(page_b, 'B', PAGE_SIZE); // Page B filled with 'B's

    // 2. Open or create the database data file
    int fd = open("db_data.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to open file");
        free(page_a);
        free(page_b);
        return 1;
    }

    // 3. Set up the I/O Vectors (iov)
    // We want to write 2 pages, so iovcnt = 2
    int iovcnt = 2;
    struct iovec iov[2];

    // Describe the first scattered chunk of memory
    iov[0].iov_base = page_a;
    iov[0].iov_len = PAGE_SIZE;

    // Describe the second scattered chunk of memory
    iov[1].iov_base = page_b;
    iov[1].iov_len = PAGE_SIZE;

    // 4. Perform the vectored write at a specific file offset (e.g., offset 0)
    // This gathers both pages and writes them sequentially as an 8KB block.
    off_t file_offset = 0;
    ssize_t bytes_written = pwritev(fd, iov, iovcnt, file_offset);

    if (bytes_written < 0) {
        perror("pwritev failed");
    } else {
        printf("Successfully gathered and wrote %ld bytes to disk!\n", bytes_written);
        printf("File size on disk is now: %d bytes\n", PAGE_SIZE * 2);
    }

    // 5. Clean up
    close(fd);
    free(page_a);
    free(page_b);
    return 0;
}
