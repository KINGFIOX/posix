#include "errors.h"

#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <unistd.h>

#define SPIN 10000000

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

long counter;

time_t end_time;

void* counter_thread(void* arg)
{
    while (time(NULL) < end_time) {
        if (int status = pthread_mutex_lock(&mutex) != 0) {
            err_abort(status, "lock mutex");
        }
        for (int spin = 0; spin < SPIN; spin++) {
            counter++;
        }
        if (int status = pthread_mutex_unlock(&mutex) != 0) {
            err_abort(status, "lock mutex");
        }
        sleep(1); /* 只是为了方便观察 */
    }
    printf("counter is %lx=n", counter);
    return NULL;
}

void* monitor_thread(void* arg)
{
    int misses = 0;
    while (time(NULL) < end_time) {
        sleep(3);
        if (int status = pthread_mutex_trylock(&mutex); status != EBUSY) {
            if (status != 0) {
                err_abort(status, "trylock mutex");
            }
            printf("Counter is %ld\n", counter / SPIN);

            if (int status = pthread_mutex_trylock(&mutex) != 0) {
                err_abort(status, "unlock mutex");
            }
        } else {
            misses++;
        }
    }
    printf("Monitor thread missed update %d times.\n", misses);
    return NULL;
}