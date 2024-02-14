#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <sys/_pthread/_pthread_mutex_t.h>
#include <sys/_pthread/_pthread_once_t.h>

#include "errors.hxx"

pthread_once_t once_block = PTHREAD_ONCE_INIT;
pthread_mutex_t mutex;

void once_init_routine(void)
{
    int status = pthread_mutex_init(&mutex, NULL);
    if (status != 0) {
        err_abort(status, "init mutex");
    }
}

void* thread_routine(void* arg)
{
    int status; /* 错误状态 */

    status = pthread_once(&once_block, once_init_routine);
    if (status != 0) {
        err_abort(status, "once init");
    }

    status = pthread_mutex_lock(&mutex);
    if (status != 0) {
        err_abort(status, "lock mutex");
    }
    /* 上锁 */
    printf("thread_routine has locked the mutex.\n");
    /* 放锁 */
    status = pthread_mutex_unlock(&mutex);
    if (status != 0) {
        err_abort(status, "unlock mutex");
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    pthread_t thread_id;

    char *input, buffer[64];

    int status;

    status = pthread_create(&thread_id, NULL, thread_routine, NULL);
    if (status != 0) {
        err_abort(status, "create thread");
    }

    status = pthread_once(&once_block, once_init_routine); /* 这里实际上无论如何都进不去的 */
    if (status != 0) {
        err_abort(status, "once init");
    }

    status = pthread_mutex_lock(&mutex);
    if (status != 0) {
        err_abort(status, "lock mutex");
    }
    /* 上锁 */
    printf("main has locked the mutex.\n");
    /* 放锁 */
    status = pthread_mutex_unlock(&mutex);
    if (status != 0) {
        err_abort(status, "unlock mutex");
    }

    status = pthread_join(thread_id, NULL);
    if (status != 0) {
        err_abort(status, "join thread");
    }

    return 0;
}