#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "errors.h"

#define ITERATIONS 10

pthread_mutex_t mutex[3] = {
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER
};

int backoff = 1; /* 是否使用回退算法 */
int yield_flag = 0; /* O, no yield; >O: yield; <O: sleep */

/**
 * @brief 向前加锁
 * 
 * @param arg 
 * @return void* 
 */
void* lock_forward(void* arg)
{
    int backoffs = 0; /* 回退的次数 */
    int status = 0;
    /* 重复很多次 */
    for (int iterate = 0; iterate < ITERATIONS; iterate++) {
        backoffs = 0;
        for (int i = 0; i < 3; i++) {
            if (i == 0) {
                status = pthread_mutex_lock(&mutex[i]);
                if (status != 0) {
                    err_abort(status, "first lock");
                }
            } else {
                if (backoff) { /* 使用回退算法 */
                    status = pthread_mutex_trylock(&mutex[i]); /*  */
                } else {
                    status = pthread_mutex_lock(&mutex[i]);
                }
                if (status == EBUSY) {
                    backoffs++; /* 回退次数++，只有trylock才会返回 */
                    printf("[forward locker backing off at %d\n]", i);
                    for (; i >= 0; i--) {
                        status = pthread_mutex_unlock(&mutex[i]);
                        if (status != 0) {
                            err_abort(status, "lock mutex");
                        }
                    }
                } else {
                    if (status != 0) {
                        err_abort(status, "lock mutex");
                    }
                    printf("[forward locker got %d\n]", i);
                }
            }
            if (yield_flag) {
                if (yield_flag > 0) {
                    sched_yield();
                } else {
                    sleep(1);
                }
            }
        }
        printf("lock forward got all locks, %d backoffs\n", backoffs);
        /* 这里是相反的顺序释放锁的 */
        pthread_mutex_unlock(&mutex[2]);
        pthread_mutex_unlock(&mutex[1]);
        pthread_mutex_unlock(&mutex[0]);
        sched_yield();
    }
    return NULL;
}

void* lock_backward(void* arg)
{
    int backoffs = 0;
    int status = 0;
    for (int iterate = 0; iterate < ITERATIONS; iterate++) {
        for (int i = 2; i >= 0; i--) {
            if (i == 2) {
                status = pthread_mutex_lock(&mutex[i]);
                if (status != 0) {
                    err_abort(status, "first lock");
                }
            } else {
                if (backoff) {
                    status = pthread_mutex_trylock(&mutex[i]);
                } else {
                    status = pthread_mutex_lock(&mutex[i]);
                }
                if (status == EBUSY) {
                    backoffs++;
                    printf("[backward locker backing off at %d\n]", i);
                    for (; i < 3; i++) {
                        status = pthread_mutex_unlock(&mutex[i]);
                        if (status != 0) {
                            err_abort(status, "backoff");
                        }
                    }
                } else {
                    if (status != 0) {
                        err_abort(status, "lock mutex");
                    }
                    printf("[backward locker got %d\n]", i);
                }
                if (yield_flag) {
                    if (yield_flag) {
                        sched_yield();
                    } else {
                        sleep(1);
                    }
                }
            }
        }
        printf("lock backward got all locks, %d backoffs\n", backoffs);
        pthread_mutex_unlock(&mutex[0]);
        pthread_mutex_unlock(&mutex[1]);
        pthread_mutex_unlock(&mutex[2]);
        sched_yield();
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    pthread_t forward, backward;
    int status;

    status = pthread_create(&forward, NULL, lock_forward, NULL);
    if (status != 0) {
        err_abort(status, "create forward");
    }
    status = pthread_create(&backward, NULL, lock_backward, NULL);
    if (status != 0) {
        err_abort(status, "create forward");
    }
    pthread_join(forward, NULL);
    pthread_join(backward, NULL);
}