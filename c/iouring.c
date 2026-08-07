#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>

#define QUEUE_DEPTH 4
#define BLOCK_SIZE  64

int main() {
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    
    char write_buf[] = "Hello from the io_uring asynchronous file engine!";
    char read_buf[BLOCK_SIZE] = {0};
    
    // 1. Initialize the io_uring instance
    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        return 1;
    }

    // 2. Open the target file descriptor
    int fd = open("test_io_uring.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        io_uring_queue_exit(&ring);
        return 1;
    }

    printf("--- Submitting Write Operation ---\n");

    // 3. Get a Submission Queue Entry (SQE) for writing
    sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE\n");
        close(fd);
        io_uring_queue_exit(&ring);
        return 1;
    }

    // Prepare an asynchronous write operation at file offset 0
    io_uring_prep_write(sqe, fd, write_buf, strlen(write_buf), 0);

    // 4. Submit the SQE to the Linux Kernel
    io_uring_submit(&ring);

    // 5. Wait for the write operation to finish via Completion Queue Entry (CQE)
    if (io_uring_wait_cqe(&ring, &cqe) < 0) {
        perror("io_uring_wait_cqe");
    } else {
        if (cqe->res < 0) {
            fprintf(stderr, "Write error: %s\n", strerror(-cqe->res));
        } else {
            printf("Successfully wrote %d bytes asynchronously.\n", cqe->res);
        }
        // Always mark the completed item as seen to free resources
        io_uring_cqe_seen(&ring, cqe);
    }

    printf("\n--- Submitting Read Operation ---\n");

    // 6. Get a fresh SQE for reading
    sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE\n");
        close(fd);
        io_uring_queue_exit(&ring);
        return 1;
    }

    // Prepare an asynchronous read operation from file offset 0
    io_uring_prep_read(sqe, fd, read_buf, BLOCK_SIZE - 1, 0);

    // 7. Submit and wait for completion loop
    io_uring_submit(&ring);
    
    if (io_uring_wait_cqe(&ring, &cqe) < 0) {
        perror("io_uring_wait_cqe");
    } else {
        if (cqe->res < 0) {
            fprintf(stderr, "Read error: %s\n", strerror(-cqe->res));
        } else {
            printf("Successfully read %d bytes asynchronously.\n", cqe->res);
            printf("File contents: \"%s\"\n", read_buf);
        }
        io_uring_cqe_seen(&ring, cqe);
    }

    // 8. Clean up system resources
    close(fd);
    io_uring_queue_exit(&ring);
    return 0;
}

//gcc -O2 -o iouring_demo iouring_demo.c -luring
//./iouring_demo
