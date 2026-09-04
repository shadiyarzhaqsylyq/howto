#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// 1. Define atomic flag
static volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
    (void)sig; // Silence unused parameter warning
    // 2. Only flip the flag (100% async-signal-safe)
    keep_running = 0;
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    printf("Running... Press Ctrl+C to test signal handling.\n");

    // 3. Loop checks the flag every iteration
    while (keep_running) {
        printf("Working...\n");
        sleep(1);
    }

    // 4. Safe, graceful cleanup happens here in normal execution context
    printf("\nCaught SIGINT! Cleaning up files, flushing buffers, and exiting cleanly.\n");
    return 0;
}
