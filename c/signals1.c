/*
Method 1
Synchronous Waiting with sigwait() (POSIX Standard)
Instead of letting the signal randomly interrupt your code, 
you block the signal and wait for it synchronously on your own terms
*/
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

