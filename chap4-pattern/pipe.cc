#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

#include "errors.h"

typedef struct stage_tag {
    pthread_mutex_t mutex;
    pthread_cond_t avail; /* 上一个 stage 数据处理完成，通过 avail 即数据可取，通知当前 stage */
    pthread_cond_t ready; /* 当前 stage 可以接受上一个 stage 的数据 */
    int data_ready;
    long data;
    pthread_t thread;
    struct stage_tag* next;
} stage_t;

typedef struct pipe_tag {
    pthread_mutex_t mutex;
    stage_t* head;
    stage_t* tail;
    int stages;
    int active;
} pipe_t;

/**
 * @brief 承担两个 stage 的连接作用
 * 
 * @param stage 传入的是next_stage
 * @param data 
 * @return int 
 */
int pipe_send(stage_t* stage, long data)
{
    int status;

    /* 上锁 */
    status = pthread_mutex_lock(&stage->mutex);
    if (status != 0) {
        return status;
    }

    /* pipe 中的数据 等待被消费。如果当前 stage 处在数据 ready 状态，肯定有问题：
		我数据都还没有处理，这数据就已经好了。当前 stage 还没就绪，等待当前 stage 就绪
	 */
    while (stage->data_ready) {
        status = pthread_cond_wait(&stage->ready, &stage->mutex);
        if (status != 0) {
            pthread_mutex_unlock(&stage->mutex);
            return status;
        }
    }

    /* send new data */
    stage->data = data;
    stage->data_ready = 1;
    status = pthread_cond_signal(&stage->avail); /* 当前 stage 的数据已经处理完成了，通知下一个 stage 数据可取 */
    if (status != 0) {
        pthread_mutex_unlock(&stage->mutex);
        return status;
    }
    status = pthread_mutex_unlock(&stage->mutex);
    return status;
}

/**
 * @brief 
 * 
 * @param arg 
 * @return void* 
 */
void* pipe_stage(void* arg)
{
    stage_t* stage = (stage_t*)arg;
    stage_t* next_stage = stage->next;

    int status;
    status = pthread_mutex_lock(&stage->mutex);
    if (status != 0) {
        err_abort(status, "lock pipe stage");
    }

    while (1) {
        while (stage->data_ready != 0) { /* 数据没好？等待！ */
            status = pthread_cond_wait(&stage->avail, &stage->mutex);
            if (status != 0) {
                err_abort(status, "waiting for previous stage");
            }
        }
        /* 数据好了？传递给下一个stage */
        pipe_send(next_stage, stage->data + 1);
        stage->data_ready = 0; /* 差不多是转移了数据的所有权的意思 */
        status = pthread_cond_signal(&stage->ready);
        if (status != 0) {
            err_abort(status, "wake next stage");
        }
    }

    return NULL;
}

int pipe_create(pipe_t* pipe, int stages)
{
    stage_t **link, *new_stage;
    link = &pipe->head;
    int status;

    /* init pipe */
    status = pthread_mutex_init(&pipe->mutex, NULL);
    if (status != 0) {
        err_abort(status, "init pip mutex");
    }
    pipe->stages = stages;
    pipe->active = 0;
    for (int pipe_index = 0; pipe_index <= stages; pipe_index++) {
        new_stage = (stage_t*)malloc(sizeof(stage_t));
        if (new_stage == NULL) {
            errno_abort("allocate stage");
        }
        status = pthread_mutex_init(&new_stage->mutex, NULL);
        if (status != 0) {
            err_abort(stages, "init stage mutex");
        }
        status = pthread_cond_init(&new_stage->avail, NULL);
        if (status != 0) {
            err_abort(stages, "init avail condition");
        }
        status = pthread_cond_init(&new_stage->ready, NULL);
        if (status != 0) {
            err_abort(stages, "init ready condition");
        }
        new_stage->data_ready = 0;
        *link = new_stage;
        link = &new_stage->next;
    }
    *link = (stage_t*)NULL;
    pipe->tail = new_stage;

    /* begin pipe ，最后一个 stage 并不会用到 */
    for (stage_t* stage = pipe->head; stage->next != NULL; stage = stage->next) {
        status = pthread_create(&stage->thread, NULL, pipe_stage, (void*)stage);
        if (status != 0) {
            err_abort(status, "create pipe stage");
        }
    }

    return 0;
}

int pipe_start(pipe_t* pipe, long value)
{
    int status;
    status = pthread_mutex_lock(&pipe->mutex);
    if (status != 0) {
        err_abort(status, "lock pipe mutex");
    }
    pipe->active++;
    status = pthread_mutex_unlock(&pipe->mutex);
    if (status != 0) {
        err_abort(status, "unlock pipe mutex");
    }
    pipe_send(pipe->head, value);
    return 0;
}

int pipe_result(pipe_t* pipe, long* result)
{
    stage_t* tail = pipe->tail;
    long value;
    int empty = 0;
    int status;

    status = pthread_mutex_lock(&pipe->mutex);
    if (status != 0) {
        err_abort(status, "lock pipe mutex");
    }
    if (pipe->active <= 0) {
        empty = 1;
    } else {
        pipe->active--;
    }

    status = pthread_mutex_unlock(&pipe->mutex);
    if (status != 0) {
        err_abort(status, "unlock pipe mutex");
    }

    if (empty) {
        return 0;
    }

    pthread_mutex_lock(&tail->mutex);
    while (!tail->data_ready) {
        pthread_cond_wait(&tail->avail, &tail->mutex);
    }
    *result = tail->data;
    tail->data_ready = 0;
    pthread_cond_signal(&tail->ready);
    pthread_mutex_unlock(&tail->mutex);

    return 1;
}

int main(int argc, char* argv[])
{
    pipe_t my_pipe;
    long value, result;
    int status;
    char line[128];

    pipe_create(&my_pipe, 10);
    printf("enter integer values, or \"=\" for next result\n");

    while (1) {
        printf("data> ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            exit(0);
        }
        if (strlen(line) <= 1) {
            continue;
        }
        if (strlen(line) <= 2 && line[0] == '=') {
            if (pipe_result(&my_pipe, &result)) {
                printf("result is %ld\n", result);
            } else {
                printf("pipe is empty\n");
            }
        } else {
            if (sscanf(line, "%ld", &value) < 1) {
                fprintf(stderr, "enter an integer value\n");
            } else {
                pipe_start(&my_pipe, value);
            }
        }
    }
}