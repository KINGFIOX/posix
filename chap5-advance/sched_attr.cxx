#include <cstdio>
#include <pthread.h>
#include <pthread/pthread_impl.h>
#include <pthread/sched.h>
#include <sched.h>
#include <unistd.h>

#include "errors.hxx"

void* thread_routine(void* arg)
{
    /**
     * struct sched_param {
     *     int sched_priority;
     *     char __opaque[__SCHED_PARAM_SIZE__];
     * };
     */

    int status;

    int my_policy;
    struct sched_param my_param;

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && !defined(sun)
    status = pthread_getschedparam(pthread_self(), &my_policy, &my_param);
    HANDLE_STATUS("get sched");
    printf("thread_routine running at %s/%d\n",
        (my_policy == SCHED_FIFO ? "FIFO" /* 先进先出 */
                                 : (my_policy == SCHED_RR ? "RR" /* 轮询 */
                                                          : (my_policy == SCHED_OTHER ? "OTHER"
                                                                                      : "unknown"))),
        my_param.sched_priority);
#else
    printf("thread_routine running\n");
#endif
    return NULL;
}

int main(void)
{
    int status;

    pthread_t thread_id;

    /* 获取默认 调度策略 */
    pthread_attr_t thread_attr;
    status = pthread_attr_init(&thread_attr);
    HANDLE_STATUS("init attr");
    int thread_policy;
    struct sched_param thread_param;

    int rr_min_priority, rr_max_priority;

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && !defined(sun)
    status = pthread_attr_getschedpolicy(&thread_attr, &thread_policy);
    HANDLE_STATUS("get policy");

    status = pthread_attr_getschedparam(&thread_attr, &thread_param);
    HANDLE_STATUS("get param");

    printf("default policy is %s, priority is %d\n",
        (thread_policy == SCHED_FIFO ? "FIFO" /* 先进先出 */
                                     : (thread_policy == SCHED_RR ? "RR" /* 轮询 */
                                                                  : (thread_policy == SCHED_OTHER ? "OTHER"
                                                                                                  : "unknown"))),
        thread_param.sched_priority);

    status = pthread_attr_setschedpolicy(&thread_attr, SCHED_RR);
    if (status != 0) {
        err_abort(status, "unable to set SCHED_RR policy");
    } else {
        rr_min_priority = sched_get_priority_min(SCHED_RR);
        if (rr_min_priority == -1) {
            errno_abort("get SCHED_RR min priority");
        }
        rr_max_priority = sched_get_priority_max(SCHED_RR);
        if (rr_max_priority == -1) {
            errno_abort("get SCHED_RR max priority");
        }
        thread_param.sched_priority += (rr_max_priority + rr_min_priority) / 2;
        printf("sched_rr priority range is %d to %dL using %d\n",
            rr_min_priority,
            rr_max_priority,
            thread_param.sched_priority);

        status = pthread_attr_setschedparam(&thread_attr, &thread_param);
        HANDLE_STATUS("set params");

        printf("creating thread at RR/%d\n", thread_param.sched_priority);

        status = pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
        HANDLE_STATUS("set inherit");
    }

#else
    printf("priority scheduling not supported\n");
#endif
    status = pthread_create(&thread_id, &thread_attr, thread_routine, NULL);
    HANDLE_STATUS("create thread");

    status = pthread_join(thread_id, NULL);
    HANDLE_STATUS("join thread");

    printf("main exiting\n");
    return 0;
}