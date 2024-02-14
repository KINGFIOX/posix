#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "errors.hxx"

static int counter;

void* thread_routine(void* arg)
{
    int status;

    int state;

    for (counter = 0;; counter++) {
        if (counter % 755 == 0) {
            status = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state); /* 关闭 取消 */
            if (status != 0) {
                err_abort(status, "disable cancel");
            }
            sleep(1);
            status = pthread_setcancelstate(state, &state); /* 恢复 */
            if (status != 0) {
                err_abort(status, "disable cancel");
            }
        } else {
            if (counter % 1000 == 0) {
                pthread_testcancel(); /*  */
            }
        }
    }
}

int main(void)
{
    pthread_t thread_id;

    void* result;

    int status;

    status = pthread_create(&thread_id, NULL, thread_routine, NULL);
    if (status != 0) {
        err_abort(status, "create thread");
    }

    sleep(2);

    status = pthread_cancel(thread_id);
    if (status != 0) {
        err_abort(status, "cancel thread");
    }

    status = pthread_join(thread_id, &result); /* 等待停止 */
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