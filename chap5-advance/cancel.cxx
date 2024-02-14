#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "errors.hxx"

static int counter;

void* thread_routine(void* arg)
{
    printf("thread_routine starting\n");
    for (counter = 0;; counter++) {
        if (counter % 1000 == 0) {
            printf("calling testcancel\n");
            pthread_testcancel();
        }
    }
    return (void*)&counter;
}

int main(int argc, char* argv[])
{
    int status;

    pthread_t thread_id;
    status = pthread_create(&thread_id, NULL, thread_routine, NULL);
    if (status != 0) {
        err_abort(status, "create thread");
    }

    sleep(2);

    printf("calling cancel\n");
    status = pthread_cancel(thread_id);
    if (status != 0) {
        err_abort(status, "cancel thread");
    }

    printf("calling join\n");
    void* result;
    status = pthread_join(thread_id, &result); /* 获取返回值 */
    /* 被 pthread_cancel 的线程，返回值是 PTHREAD_CANCELED */

    if (status != 0) {
        err_abort(status, "join thread");
    }

    if (result == PTHREAD_CANCELED) {
        printf("thread canceled at iteration %d\n", counter);
    } else {
        printf("thread was not canceled\n");
    }

    return 0;
}