#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
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

    // 有线程被加入到 wq 中
    if (wq->counter > 0) {

        wq->quit = 1; // 设置标志位，要求线程关闭自己

        if (wq->idle > 0) {
            status = pthread_cond_broadcast(&wq->cv);
            /* 广播 cv，唤醒 任何的 idle 或者 等待的 engine 线程。
             * 当发现没有更多工作的时候，他们将关闭自己
             */
            if (status != 0) {
                pthread_mutex_unlock(&wq->mutex);
                return status;
            }
        }

        while (wq->counter > 0) { /* 等到：所有线程退出 */
            status = pthread_cond_wait(&wq->cv, &wq->mutex);
            if (status != 0) {
                pthread_mutex_unlock(&wq->mutex);
                return status;
            }
        }
    }

    /* 解锁 */
    status = pthread_mutex_unlock(&wq->mutex);
    if (status != 0) {
        return status;
    }

    status = pthread_mutex_destroy(&wq->mutex);
    status1 = pthread_cond_destroy(&wq->cv);
    status2 = pthread_attr_destroy(&wq->attr);
    // FIXME

    return (status ? status : (status1 ? status1 : status2));
}

int workq_add(workq_t* wq, void* element)
{

    int status;

    if (wq->valid != WORKQ_VALID) {
        return EINVAL;
    }

    workq_ele_t* item = (workq_ele_t*)malloc(sizeof(workq_ele_t));
    if (item == NULL) {
        return ENOMEM;
    }
    item->data = element;
    item->next = NULL;
    status = pthread_mutex_lock(&wq->mutex);
    if (status != 0) {
        free(item);
        return status;
    }

    /* 尾插 */
    if (wq->first == NULL) {
        wq->first = item;
    } else {
        wq->last->next = item;
    }
    wq->last = item;

    /* 如果有空闲的线程，那么 */
    pthread_t id;
    if (wq->idle > 0) {
        status = pthread_cond_signal(&wq->cv);
        if (status != 0) {
            pthread_mutex_unlock(&wq->mutex);
            return status;
        }
    } else if (wq->counter < wq->parallelism) {
        printf("creating new worker\n");
        status = pthread_create(&id, &wq->attr, workq_server, (void*)wq);
        if (status != 0) {
            pthread_mutex_unlock(&wq->mutex);
            return status;
        }
        wq->counter++;
    }
    pthread_mutex_unlock(&wq->mutex);
    return 0;
}

static void* workq_server(void* arg)
{
    int status;

    workq_t* wq = (workq_t*)arg;

    printf("a worker is starting\n");

    status = pthread_mutex_lock(&wq->mutex);
    if (status != 0) {
        return NULL;
    }

    while (1) {
        int timedout = 0;
        printf("worker waiting for worker\n");

        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 2;

        while (wq->first == NULL && !wq->quit) {
            status = pthread_cond_timedwait(&wq->cv, &wq->mutex, &timeout);
            if (status == ETIMEDOUT) {
                printf("worker wait timed out\n");
                timedout = 1;
                break;
            } else if (status != 0) {
                printf("worker wait failed, %d (%s)", status, strerror(status));
                wq->counter--;
                pthread_mutex_unlock(&wq->mutex);
                return NULL;
            }
        }
        printf("work queue: 0x%p, quit: %d\n", wq->first, wq->quit);

        workq_ele_t* we = wq->first;

        if (we != NULL) {
            wq->first = we->next;
            if (wq->last == we) {
                wq->last = NULL;
            }
            status = pthread_mutex_unlock(&wq->mutex);
            if (status != 0) {
                return NULL;
            }
            printf("worker calling engine\n");
            wq->engine(we->data);
            free(we);
            status = pthread_mutex_lock(&wq->mutex);
            if (status != 0) {
                return NULL;
            }
        }

        /* 只有： quit 的时候才会进入到这里 */
        if (wq->first == NULL && wq->quit) {
            printf("worker shutting down\n");
            wq->counter--;
            if (wq->counter == 0) {
                pthread_cond_broadcast(&wq->cv);
                // 这个 broadcast 对应于上面 destroy
            }
            pthread_mutex_unlock(&wq->mutex);
            return NULL;
        }

        if (wq->first == NULL && timedout) {
            printf("engine terminating due to timeoue.\n");
            wq->counter--;
            break;
        }
    }

    pthread_mutex_unlock(&wq->mutex);
    printf("wroker exiting\n");
    return NULL;
}