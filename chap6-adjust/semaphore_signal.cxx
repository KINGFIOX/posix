#include <bits/types/sigset_t.h>
#include <bits/types/struct_itimerspec.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "errors.hxx"

sem_t semaphore;

/**
 * @brief 信号捕捉函数，执行 V 操作
 *
 * @param sig
 */
void signal_catcher(int sig)
{
    if (sem_post(&semaphore) == -1) {
        errno_abort("post semaphore");
    }
}

void* sem_waiter(void* arg)
{
    long number = reinterpret_cast<long>(arg);

    /** 每个线程 P 5 次。如果不够 P ，那么 sem_wait 会阻塞
     */
    for (int counter = 1; counter <= 5; counter++) {
        while (sem_wait(&semaphore) == -1) {
            if (errno != EINTR) {
                errno_abort("wait on semaphore");
            }
        }
        printf("%d waking (%d)...\n", number, counter);
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    int status;

    pthread_t sem_waiters[5];

#if !defined(_POSIX_SEMAPHORES) || !defined(_POSIX_TIMERS)

#if !defined(_POSIX_SEMAPHORES)
    printf("this system does not support posix semaphores\n");
#endif

#if !defined(_POSIX_TIMERS)
    printf("this system does not support posix timers\n");
#endif

    return -1;

#else

    /**
     * pshared：如果是 0，那么所有的线程都可以共享；如果是 n，那么最多只有 n 个线程共享
     * value 资源的个数（P、V）
     */
    sem_init(&semaphore, 0, 0);

    for (int thread_count = 0; thread_count < 5; thread_count++) {
        status = pthread_create(&sem_waiters[thread_count],
            NULL, sem_waiter, (void*)thread_count);
        HANDLE_STATUS("create thread");
    }

    struct sigevent sig_event;
    sig_event.sigev_value.sival_int = 0; /* 这里是自定义的数据，可以在 信号处理函数中 访问 */
    sig_event.sigev_signo = SIGRTMIN; /* 发送实时信号的最小值 */
    sig_event.sigev_notify = SIGEV_SIGNAL; /* 时间到了，发送信号给 进程 */

    timer_t timer_id;
    if (timer_create(CLOCK_REALTIME, &sig_event, &timer_id) == -1) {
        errno_abort("create timer");
    }

    /* 设置 信号的 处理函数。处理函数的内容是：执行 V 操作 */
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGRTMIN);
    struct sigaction sig_action;
    sig_action.sa_handler = signal_catcher;
    sig_action.sa_mask = sig_mask;
    sig_action.sa_flags = 0;
    if (sigaction(SIGRTMIN, &sig_action, NULL) == -1) {
        errno_abort("set signal action");
    }

    struct itimerspec timer_val;
    timer_val.it_value.tv_sec = 2;
    timer_val.it_value.tv_nsec = 0;
    timer_val.it_interval.tv_sec = 2;
    timer_val.it_interval.tv_nsec = 0;
    if (timer_settime(timer_id, 0, &timer_val, NULL) == -1) {
        errno_abort("set timer");
    }

    /* 相当于是：随着时间的推移，资源越来越多 */

    for (int thread_count = 0; thread_count < 5; thread_count++) {
        status = pthread_join(sem_waiters[thread_count], NULL);
        HANDLE_STATUS("join thread");
    }

    return 0;

#endif
}