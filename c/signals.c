#include <stdio.h>
#include <signal.h>
#include <unistd.h>

// volatile sig_atomic_t guarantees lock-free, atomic reads/writes
static volatile sig_atomic_t keep_running = 1;

void sig_handler(int sig) {
    keep_running = 0; // That's it! 1 line.
}

int main() {
    signal(SIGINT, sig_handler);

    while (keep_running) {
        // Do your main work here
        sleep(1);
    }

    // Perform full, safe cleanup here
    printf("Cleaning up and exiting safely...\n");
    return 0;
}
