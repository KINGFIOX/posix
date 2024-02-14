#include "errors.h"
#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef struct my_struct_tag {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int value;
} my_struct_t;

my_struct_tag data = {
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0
};

int hibernation = 1;

void* wait_thread(void* arg)
{
    sleep(hibernation);
    if (int status = pthread_mutex_lock(&data.mutex) != 0) {
        err_abort(status, "lock mutex");
    }
    data.value = 1;
    if (int status = pthread_cond_signal(&data.cond) != 0) {
        err_abort(status, "signal condition");
    }
    if (int status = pthread_mutex_unlock(&data.mutex) != 0) {
        err_abort(status, "unlock mutex");
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc > 1) {
        hibernation = atoi(argv[1]);
    }

    pthread_t wait_thread_id;
    if (int status = pthread_create(&wait_thread_id, NULL, wait_thread, NULL) != 0) {
        err_abort(status, "create wait thread");
    }

    struct timespec timeout;
    timeout.tv_sec = time(NULL) + 2;
    timeout.tv_nsec = 0;

    if (int status = pthread_mutex_lock(&data.mutex) != 0) {
        err_abort(status, "lock mutex");
    }

    while (data.value == 0) {
        if (int status = pthread_cond_timedwait(&data.cond, &data.mutex, &timeout) == ETIMEDOUT) {
            printf("condition wait timed out.\n");
            break;
        } else if (status != 0) {
            err_abort(status, "wait on condition");
        }
    }

    if (data.value != 0) {
        printf("condition was signaled.\n");
    }
    if (int status = pthread_mutex_unlock(&data.mutex) != 0) {
        err_abort(status, "unlock mutex");
    }
    return 0;
}