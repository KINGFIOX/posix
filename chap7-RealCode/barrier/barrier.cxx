#include <ctime>
#include <pthread.h>

#include "barrier.hxx"
#include "errors.hxx"

int barrier_init(barrier_t* barrier, int count)
{
    int status;

    barrier->threshold = barrier->counter = count; // 设置 通过 barrier 通过的 阈值
    status = pthread_mutex_init(&barrier->mutex, NULL);
    if (status != 0) {
        return status;
    }

    status = pthread_cond_init(&barrier->cv, NULL);
    if (status != 0) {
        pthread_mutex_destroy(&barrier->mutex);
        return status;
    }
    barrier->valid = BARRIER_VALID;

    return 0;
}

int barrier_destroy(barrier_t* barrier)
{
    int status1, status2;

    /* 先检查一遍：是否有效，是否有被初始化 */
    if (barrier->valid != BARRIER_VALID) {
        return EINVAL;
    }

    status1 = pthread_mutex_lock(&barrier->mutex);
    if (status1 != 0) {
        return status1; /* 但是有个问题，就是没有解锁 */
    }

    /* 看一下是不是有线程在等待 */
    if (barrier->counter != barrier->threshold) {
        pthread_mutex_unlock(&barrier->mutex);
        return EBUSY;
    }

    barrier->valid = 0; /* 设置为无效 */
    status1 = pthread_mutex_unlock(&barrier->mutex);
    if (status1 != 0) {
        return status1;
    }

    /* 释放资源 */
    status1 = pthread_mutex_destroy(&barrier->mutex);
    status2 = pthread_cond_destroy(&barrier->cv);

    return (status1 != 0 ? status1 : status2);
}

/**
 * 我们这个设计的是：counter 一开始是：threshold，然后 --
 * 当 counter == 0 的时候，通知线程通过
 *
 * 返回 -1 ： 领头线程：broadcast 的线程
 */

int barrier_wait(barrier_t* barrier)
{
    int status;

    /* 检查是否有效 */
    if (barrier->valid != BARRIER_VALID) {
        return EINVAL;
    }

    status = pthread_mutex_lock(&barrier->mutex);
    if (status != 0) {
        return status;
    }
    /* 上锁 */

    unsigned long cycle = barrier->cycle;

    int cancel;

    if (--barrier->counter == 0) { // 这里有 --，我的评价是：代码风格很 鬼斧神工
        barrier->cycle++;
        barrier->counter = barrier->threshold; // 恢复

        status = pthread_cond_broadcast(&barrier->cv);
        if (status == 0) {
            status = -1;
        }
    } else {
        /* 关 中断（不能让他取消线程） */
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel);

        /* 当某一个线程 进入到了 barrier->counter == 0 的时候，cycle 会发生改变 */
        /* 这个时候，不会继续等待。这种方式可以防止 虚假唤醒的现象发生 */
        while (cycle == barrier->cycle) {
            status = pthread_cond_wait(&barrier->cv, &barrier->mutex);
            if (status != 0) {
                break;
            }
        }
    }

    /* 解锁 */
    status = pthread_mutex_unlock(&barrier->mutex);
    if (status != 0) {
        return status;
    }

    return status;
}