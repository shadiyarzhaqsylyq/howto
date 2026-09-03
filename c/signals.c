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

/*
Method 1
Synchronous Waiting with sigwait() (POSIX Standard)
Instead of letting the signal randomly interrupt your code, 
you block the signal and wait for it synchronously on your own terms

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int main() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);  // We want to catch Ctrl+C

    // 1. Block the signal so it never triggers an interrupt/handler
    sigprocmask(SIG_BLOCK, &mask, NULL);

    printf("Waiting for Ctrl+C... (no messy handlers used!)\n");

    int caught_signal;
    // 2. Sleep cleanly until the signal arrives
    sigwait(&mask, &caught_signal);

    // 3. Normal, safe code runs here!
    printf("\nReceived signal %d. Now doing safe cleanup with normal printf/malloc...\n", caught_signal);

    return 0;
}
Why it is better
No signal handler function needed at all.
Code executes linearly and predictably
You can safely use any standard lib function(printf,malloc, etc.)

Method 2
The Dedicated Signal Thread(Multithreaded Apps)
Block all signals in all worker threads
Spaw a single, dedicated management thread that runs sigwait() in loop

void* signal_thread(void* arg) {
    sigset_t mask;
    // ... setup mask for SIGINT, SIGTERM ...
    int sig;
    while (1) {
        sigwait(&mask, &sig);
        // Handle shutdown gracefully, trigger condition variables, etc.
    }
}

Method 3
Existing loop that needs to stop

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

*/
