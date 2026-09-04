#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// Signal handler function
void handle_sigint(int sig) {
    // Note: write() is async-signal-safe, printf() is not.
    const char msg[] = "\nCaught SIGINT (Ctrl+C)! Exiting gracefully...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    _exit(0);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Register handler for SIGINT
    sigaction(SIGINT, &sa, NULL);

    printf("Running... Press Ctrl+C to test signal handling.\n");
    while (1) {
        sleep(1);
    }
    return 0;
}


