#include <climits>
#include <cstddef>
#include <cstdio>
#include <limits.h>
#include <pthread.h>
#include <pthread/sched.h>
#include <sys/_pthread/_pthread_attr_t.h>

#include "errors.hxx"

void* thread_routine(void* arg)
{
    printf("the thread is here\n");
    return NULL;
}

int main(int argc, char* argv[])
{
    pthread_t thread_id;

    pthread_attr_t thread_attr;

    /**
     * struct sched_param {
     * 		int sched_priority;
     * 		char __opaque[__SCHED_PARAM_SIZE__];
     * };
     * __SCHED_PARAM_SIZE__ 在我的电脑上是 4
     */

    struct sched_param thread_param;

    size_t stack_size;

    int status;

    status = pthread_attr_init(&thread_attr);
    if (status != 0) {
        err_abort(status, "create attr");
    }

    stack_size = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    if (status != 0) {
        err_abort(status, "set detach");
    }

    /* #define	_POSIX_THREAD_ATTR_STACKSIZE	200112L		TSS 在我电脑上 */
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
    stack_size = pthread_attr_getstacksize(&thread_attr, &stack_size);
    if (status != 0) {
        err_abort(status, "get stack size");
    }
    printf("default stack size is %u; minimum is %u\n", stack_size, PTHREAD_STACK_MIN);

    status = pthread_attr_setdetachstate(&thread_attr, PTHREAD_STACK_MIN * 2);
    if (status != 0) {
        err_abort(status, "set stack size");
    }
#endif

    status = pthread_create(&thread_id, &thread_attr, thread_routine, NULL);
    if (status != 0) {
        err_abort(status, "pthread create");
    }

    printf("main exiting.\n");
    pthread_exit(NULL);

    return 0;
}