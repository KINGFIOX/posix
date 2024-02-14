#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <pthread/sched.h>
#include <sched.h>
#include <unistd.h>

#include "errors.hxx"

#define THREADS 5

typedef struct thread_tag {
    int index;
    pthread_t id;
} thread_t;

thread_t threads[THREADS];

int rr_min_priority;

void* thread_routine(void* arg)
{
    thread_t* self = (thread_t*)arg;

    int my_policy;

    struct sched_param my_param;

    int status;

    /**
     * @brief 设置优先级
     *		优先级高的在前面，可以看见，这个优先级越来越高了
     */
    my_param.sched_priority = rr_min_priority + self->index;
    printf("thread %d will set sched_fifo priority %d\n",
        self->index,
        my_param.sched_priority);
    status = pthread_setschedparam(self->id, SCHED_RR, &my_param);
    HANDLE_STATUS("set sched");

    /* 打印优先级 */
    status = pthread_getschedparam(self->id, &my_policy, &my_param);
    HANDLE_STATUS("get sched");
    printf("thread_rountine %d running at %s/%d\n", self->index,
        (my_policy == SCHED_FIFO ? "FIFO"
                                 : (my_policy == SCHED_RR ? "RR"
                                                          : (my_policy == SCHED_OTHER ? "OTHER"
                                                                                      : "unknown"))),
        my_param.sched_priority);

    return NULL;
}

int main(int argc, char* argv[])
{
    int status;
    rr_min_priority = sched_get_priority_min(SCHED_RR);
    if (rr_min_priority == -1) {
#ifdef sun
        if (errno == ENOSYS) {
            fprintf(stderr, "SCHED_RR is not supported.\n");
            exit(0);
        }
#endif
        errno_abort("get SCHED_RR min priority");
    }
    for (int count = 0; count < THREADS; count++) {
        threads[count].index = count;
        status = pthread_create(&threads[count].id, NULL, thread_routine, (void*)&threads[count]);
        HANDLE_STATUS("create thread");
    }

    for (int count = 0; count < THREADS; count++) {
        status = pthread_join(threads[count].id, NULL);
        HANDLE_STATUS("join thread");
    }
    printf("main exiting\n");
    return 0;
}