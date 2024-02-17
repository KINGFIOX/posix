#include <cstddef>
#include <pthread.h>
#include <time.h>

#include "errors.hxx"
#include "workq.hxx"

int workq_init(workq_t* wq, int threads, void (*engin)(void*))
{
    int status = 0;

    /* 初始化属性 */
    status = pthread_attr_init(&wq->attr);
    if (status != 0) {
        return status;
    }

    /* 将属性设置为 detach */
    status = pthread_attr_setdetachstate(&wq->attr, PTHREAD_CREATE_DETACHED);
    if (status != 0) {
        pthread_attr_destroy(&wq->attr);
        return status;
    }

    status = pthread_mutex_init(&wq->mutex, NULL);
    if (status != 0) {
        pthread_attr_destroy(&wq->attr);
        return status;
    }

    status = pthread_cond_init(&wq->cv, NULL);
    if (status != 0) {
        pthread_mutex_destroy(&wq->mutex);
        pthread_attr_destroy(&wq->attr);
        return status;
    }

    wq->quit = 0;
    wq->first = wq->last = NULL;
    wq->parallelism = threads; // 最大 线程数
    wq->counter = 0; // 还没有 创建的线程呢
    wq->idle = 0; // 应该是：parallelism >= counter >= idle
    wq->engine = engin;
    wq->valid = WORKQ_VALID;

    return status;
}

int workq_destroy(workq_t* wq)
{
    int status, status1, status2;

    if (wq->valid != WORKQ_VALID) {
        return EINVAL;
    }

    status = pthread_mutex_lock(&wq->mutex);
    if (status != 0) {
        return status;
    }
    /* 上锁 */

    /* 解锁 */
    status = pthread_mutex_unlock(&wq->mutex);
    if (status != 0) {
        return status;
    }

    status = pthread_mutex_destroy(&wq->mutex);
    status1 = pthread_cond_destroy(&wq->cv);
    status2 = pthread_attr_destroy(&wq->attr);
}

extern int workq_add(workq_t* wq, void* data);