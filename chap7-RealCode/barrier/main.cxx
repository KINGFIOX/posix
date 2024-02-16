#include <cstddef>
#include <cstdio>
#include <pthread.h>

#include "barrier.hxx"
#include "errors.hxx"

#define THREADS 5
#define ARRAY 6
#define INLOOPS 1000
#define OUTLOOPS 10

typedef struct thread_tag {
    pthread_t thread_id;
    int number;
    int increment;
    int arr[ARRAY];
} thread_t;

barrier_t barrier;
thread_t thread[THREADS];

/**
 * @brief 我的评价是：测试代码奇奇怪怪
 *
 */
void* thread_routine(void* arg)
{
    int status;

    thread_t* self = (thread_t*)arg;

    for (int out_loop; out_loop < OUTLOOPS; out_loop++) {
        status = barrier_wait(&barrier);
        if (status > 0) {
            HANDLE_STATUS("wait on barrier");
        }

        for (int in_loop = 0; in_loop < INLOOPS; in_loop++) {
            for (int count = 0; count < ARRAY; count++) {
                self->arr[count] += self->increment;
            }
        }

        status = barrier_wait(&barrier);
        if (status > 0) {
            HANDLE_STATUS("wait on barrier");
        }

        if (status == -1) {
            for (int thread_num = 0; thread_num < THREADS; thread_num++) {
                thread[thread_num].increment += 1;
            }
        }
    }

    return NULL;
}

int main(int arg, char* argv[])
{
    int status;

    barrier_init(&barrier, THREADS);

    for (int thread_count = 0; thread_count < THREADS; thread_count++) {
        thread[thread_count].increment = thread_count;
        thread[thread_count].number = thread_count;

        for (int array_count = 0; array_count < ARRAY; array_count++) {
            thread[thread_count].arr[array_count] = array_count + 1;
        }

        status = pthread_create(&thread[thread_count].thread_id, NULL,
            thread_routine, (void*)&thread[thread_count]);
        HANDLE_STATUS("create thread");
    }

    for (int thread_count = 0; thread_count < THREADS; thread_count++) {
        status = pthread_join(thread[thread_count].thread_id, NULL);
        HANDLE_STATUS("join thread");

        printf("%02d: (%d) ", thread_count, thread[thread_count].increment);

        for (int array_count; array_count < ARRAY; array_count++) {
            printf("%010u ", thread[thread_count].arr[array_count]);
        }
        printf("\n");
    }
}
