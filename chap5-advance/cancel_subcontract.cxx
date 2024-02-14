#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "errors.hxx"

#define THREADS 5

typedef struct team_tag {
    int join_i; /* join index，记录了 与 master 线程连接的 最后一个 worker 线程的 index */
    pthread_t workers[THREADS]; /* thread id */
} team_t;

void* worker_routine(void* arg)
{
    for (int counter = 0;; counter++) {
        if (counter % 1000 == 0) {
            pthread_testcancel();
        }
    }
}

void cleanup(void* arg)
{
    team_t* team = (team_t*)arg;
    int status;
    for (int count = team->join_i; count < THREADS; count++) {
        status = pthread_cancel(team->workers[count]);
        if (status != 0) {
            err_abort(status, "cancel worker");
        }

        status = pthread_detach(team->workers[count]);
        if (status != 0) {
            err_abort(status, "detach worker");
        }
        printf("cleanup: canceled %d\n", count);
    }
}

/**
 * @brief 创建 master 线程
 *
 * @param arg
 * @return void*
 */
void* thread_routine(void* arg)
{
    team_t team;

    void* result;

    int status;

    for (int count = 0; count < THREADS; count++) {
        status = pthread_create(&team.workers[count], NULL, worker_routine, NULL);
        if (status != 0) {
            err_abort(status, "create worker");
        }
    }

    pthread_cleanup_push(cleanup, (void*)&team);

    for (team.join_i = 0; team.join_i < THREADS; team.join_i++) {
        status = pthread_join(team.workers[team.join_i], &result);
        if (status != 0) {
            err_abort(status, "join worker");
        }
    }

    pthread_cleanup_pop(0); /* 仅移除，不执行 */
    return NULL;
}

int main(void)
{
    pthread_t thread_id;
    int status;

    status = pthread_create(&thread_id, NULL, thread_routine, NULL);
    if (status != 0) {
        err_abort(status, "create team");
    }

    sleep(5);

    status = pthread_cancel(thread_id);
    if (status != 0) {
        err_abort(status, "cancel thread");
    }

    status = pthread_join(thread_id, NULL);
    if (status != 0) {
        err_abort(status, "join team");
    }
    return 0;
}