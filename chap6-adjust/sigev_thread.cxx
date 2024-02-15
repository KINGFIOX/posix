#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <sys/signal.h>
#include <sys/time.h>

#include "errors.hxx"

/**
 * @brief 定时器标识符，用于表示创建的定时器 实例
 *
 */
timer_t timer_id; /* 我的评价是 mac 没有 timer */

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int counter = 0;

/**
 * @brief
 *
 * @param arg
 */
void timer_thread(void* arg)
{
    int status;

    status = pthread_mutex_lock(&mutex);
    HANDLE_STATUS("lock mutex");

    if (++counter >= 5) {
        status = pthread_cond_signal(&cond);
        HANDLE_STATUS("signal condition");
    }

    status = pthread_mutex_unlock(&mutex);
    HANDLE_STATUS("unlock mutex");

    printf("timer %d\n", counter);
}

int main(int argc, char* argv[])
{
    int status;

    /**
     * 创建定时器到期时，应该触发的事件
     *
     */
    struct sigevent se;
    se.sigev_notify = SIGEV_THREAD;
    se.sigev_value.sival_ptr = &timer_id; /* 可以知道：是哪一个定时器 触发了 */
    se.sigev_notify_function = reinterpret_cast<void (*)(__sigval_t)>(timer_thread);
    se.sigev_notify_attributes = NULL;

    /* 设置定时器时间 */
    struct itimerspec ts;
    ts.it_value.tv_sec = 5; /* 首次触发的时间 */
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 5; /* 间隔时间为 5 秒 */
    ts.it_interval.tv_nsec = 0;

    printf("creating timer\n");

    /* 绑定定时器 和 事件 */
    status = timer_create(CLOCK_REALTIME, &se, &timer_id);
    if (status == -1) {
        errno_abort("set timer");
    }

    status = pthread_mutex_lock(&mutex);
    HANDLE_STATUS("lock mutex");
    /* 上锁 */
    while (counter < 5) {
        status = pthread_cond_wait(&cond, &mutex);
        HANDLE_STATUS("wait on condition");
    }
    /* 解锁 */
    status = pthread_mutex_unlock(&mutex);
    HANDLE_STATUS("unlock mutex");

    return 0;
}
