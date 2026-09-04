#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
//Multithreaded app
void* signal_thread_func(void* arg) {
    sigset_t* set = (sigset_t*)arg;
    int sig;
    sigwait(set, &sig);
    
    printf("\n[Signal Thread] Caught signal %d! Performing full safe cleanup...\n", sig);
    // Safe to do complex cleanup here (free memory, close files, etc.)
    _exit(0);
}

int main() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    // Block signals in main thread (all child threads inherit this mask)
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    pthread_t sig_thread;
    pthread_create(&sig_thread, NULL, signal_thread_func, &mask);

    // Main thread is free to do normal work without getting interrupted
    while (1) {
        printf("Working...\n");
        sleep(2);
    }
    return 0;
}
