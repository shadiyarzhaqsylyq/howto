#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <aio.h>
#include <errno.h>

#define MAX_EVENTS 5
#define BUFFER_SIZE 64

int main() {
    int epoll_fd, file_fd, efd;
    struct epoll_event ev, events[MAX_EVENTS];
    struct aiocb aio;
    char buffer[BUFFER_SIZE] = {0};

    // 1. Open the file 
    file_fd = open("test_epoll_aio.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (file_fd < 0) { perror("open"); return 1; }

    // Seed the file with data to read later
    write(file_fd, "Hello from the Async Epoll Architecture!", 39);

    // 2. Create an eventfd to bridge AIO to epoll
    efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) { perror("eventfd"); close(file_fd); return 1; }

    // 3. Setup the epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create1"); close(efd); close(file_fd); return 1; }

    // Monitor the eventfd for read events
    ev.events = EPOLLIN;
    ev.data.fd = efd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, efd, &ev) < 0) {
        perror("epoll_ctl");
        goto cleanup;
    }

    // 4. Configure the Asynchronous I/O Control Block (aiocb)
    memset(&aio, 0, sizeof(struct aiocb));
    aio.aio_fildes = file_fd;
    aio.aio_buf = buffer;
    aio.aio_nbytes = BUFFER_SIZE - 1;
    aio.aio_offset = 0;

    // Signal our eventfd when the file read completes
    aio.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    aio.aio_sigevent.sigev_signo = efd; 

    printf("Submitting asynchronous file read...\n");
    if (aio_read(&aio) < 0) {
        perror("aio_read");
        goto cleanup;
    }

    // 5. The Epoll Event Loop
    printf("Entering epoll event loop, waiting for file I/O thread...\n");
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (nfds < 0) { perror("epoll_wait"); goto cleanup; }

    for (int i = 0; i < nfds; i++) {
        if (events[i].data.fd == efd) {
            uint64_t finished_operations;
            // Read from eventfd to reset its counter state
            read(efd, &finished_operations, sizeof(finished_operations));

            // Verify if the AIO block completed successfully
            if (aio_error(&aio) == 0) {
                ssize_t bytes_read = aio_return(&aio);
                printf("Epoll caught completion notification!\n");
                printf("Read %zd bytes dynamically: \"%s\"\n", bytes_read, buffer);
            } else {
                fprintf(stderr, "Asynchronous file I/O failed.\n");
            }
        }
    }

cleanup:
    close(epoll_fd);
    close(efd);
    close(file_fd);
    return 0;
}
