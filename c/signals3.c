#include <stdio.h>
#include <signal.h>
#include <unistd.h>
//singlethreaded
static volatile sig_atomic_t g_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    g_running = 0; // Async-signal-safe: only setting a flag
}

int main() {
    struct sigaction sa = { .sa_handler = handle_sigint };
    sigaction(SIGINT, &sa, NULL);

    while (g_running) {
        // Do normal work here
        sleep(1);
    }

    // Safely clean up outside the signal handler
    printf("\nExiting cleanly using printf/malloc/free...\n");
    return 0;
}
