#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "errors.hxx"

#define THREADS 5

/**
 * @brief 控制结构（control）被所有的线程使用来维护共享同步对象 和 不变量。
 *
 */
typedef struct control_tag {
    int counter; // 记录运行的线程的个数
    int busy;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
} control_t;

/**
 * @brief 全局变量
 *
 */
control_t control = { 0, 1, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER };

void cleanup_handler(void* arg)
{
    control_t* st = (control_t*)arg;
    st->counter--;
    printf("cleanup_handler: counter==%d\n", st->counter);

    int status = pthread_mutex_unlock(&st->mutex);
    if (status != 0) {
        err_abort(status, "unlock in cleanup handler");
    }
}

void* thread_routine(void* arg)
{
    int status;

    pthread_cleanup_push(cleanup_handler, (void*)&control);

    status = pthread_mutex_lock(&control.mutex);
    if (status != 0) {
        err_abort(status, "mutex lock");
    }
    control.counter++;

    while (control.busy) { // 这里十几单是 while 1
        status = pthread_cond_wait(&control.cv, &control.mutex);
        if (status != 0) {
            err_abort(status, "wait on condition");
        }
    }

    pthread_cleanup_pop(1);
    return NULL;
}

int main(int argc, char* argv[])
{
    pthread_t thread_ids[THREADS];

    void* result;

    int stauts;

    for (int count = 0; count < THREADS; count++) {
        stauts = pthread_create(&thread_ids[count], NULL, thread_routine, NULL);
        if (stauts != 0) {
            err_abort(stauts, "create thread");
        }
    }

    sleep(2);

    for (int count = 0; count < THREADS; count++) {
        stauts = pthread_cancel(thread_ids[count]);
        if (stauts != 0) {
            err_abort(stauts, "cancel thread");
        }

        stauts = pthread_join(thread_ids[count], &result);
        if (stauts != 0) {
            err_abort(stauts, "join thread");
        }
        if (result == PTHREAD_CANCELED) {
            printf("thread %d canceled\n", count);
        } else {
            printf("thread %d was not canceled\n", count);
        }
    }
    return 0;
}