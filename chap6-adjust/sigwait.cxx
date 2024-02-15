#include <cstdio>
#include <pthread.h>
#include <signal.h>
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "errors.hxx"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int interrupted = 0;

/**
 * @brief 这个在 darwin 下是 uint32，
 */
sigset_t signal_set;

void* singal_waiter(void* arg)
{
    int status;

    int sig_number;

    int signal_count = 0;

    while (1) {
        /* 实际上 signal_set 只有 SIGINT */
        sigwait(&signal_set, &sig_number);
        if (sig_number == SIGINT) {
            printf("got sigint (%d of 5)\n", ++signal_count);
            if (signal_count >= 5) {
                status = pthread_mutex_lock(&mutex);
                HANDLE_STATUS("lock mutex");

                interrupted = 1;

                status = pthread_cond_signal(&cond);
                HANDLE_STATUS("signal condition");

                status = pthread_mutex_unlock(&mutex);
                HANDLE_STATUS("unlock mutex");

                break;
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    pthread_t signal_thread_id;

    int status;

    /* sigwait 之前，应该要把 等待的信号阻塞掉 */
    /* 所有的线程，都会从 创造者继承 信号掩码 */
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    status = pthread_sigmask(SIG_BLOCK, &signal_set, NULL);
    HANDLE_STATUS("set signal mask");

    /* 等待被打断 */
    status = pthread_mutex_lock(&mutex);
    HANDLE_STATUS("lock mutex");
    /* 上锁 */
    while (!interrupted) {
        status = pthread_cond_wait(&cond, &mutex); /* 主要是，主线程会阻塞到这里 */
        HANDLE_STATUS("wait for interrupt");
    }
    /* 解锁 */
    status = pthread_mutex_unlock(&mutex);
    HANDLE_STATUS("unlock mutex");

    printf("main terminating with SIGINT\n");

    return 0;
}