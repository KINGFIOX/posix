#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_THREADS 5
#define TASK_QUEUE_SIZE 10

typedef struct task {
    void (*function)(void* arg);
    void* arg;
} task_t;

typedef struct thread_pool {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t* threads;
    task_t* tasks;
    int thread_count;
    int task_queue_size;
    int head;
    int tail;
    int count;
    int shutdown;
    int started;
} thread_pool_t;

int thread_pool_init(thread_pool_t* pool, int num_threads, int num_tasks);
int thread_pool_add(thread_pool_t* pool, void (*function)(void*), void* arg);
int thread_pool_destroy(thread_pool_t* pool, int graceful);
void* thread_do_work(void* arg);

void task_function(void* arg)
{
    printf("Thread %lu is working on task %d\n", pthread_self(), *(int*)arg);
    free(arg);
}

int main(int argc, char** argv)
{
    thread_pool_t pool;
    int num_threads = MAX_THREADS;
    int num_tasks = TASK_QUEUE_SIZE;

    // Initialize thread pool
    if (thread_pool_init(&pool, num_threads, num_tasks) != 0) {
        printf("Failed to initialize thread pool!\n");
        return 1;
    }

    // Add tasks to the pool
    for (int i = 0; i < TASK_QUEUE_SIZE; ++i) {
        int* arg = (int*)malloc(sizeof(int));
        *arg = i;
        thread_pool_add(&pool, task_function, arg);
    }

    // Destroy the pool after some time
    sleep(5);
    thread_pool_destroy(&pool, 1);

    return 0;
}

int thread_pool_init(thread_pool_t* pool, int num_threads, int num_tasks)
{
    if (pool == NULL || num_threads <= 0 || num_tasks <= 0) {
        return -1;
    }

    // Initialize the fields
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    pool->tasks = (task_t*)malloc(sizeof(task_t) * num_tasks);
    pool->thread_count = num_threads;
    pool->task_queue_size = num_tasks;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;

    // Initialize mutex and condition variable
    if (pthread_mutex_init(&(pool->lock), NULL) != 0 || pthread_cond_init(&(pool->notify), NULL) != 0 || pool->threads == NULL || pool->tasks == NULL) {
        return -1;
    }

    // Start worker threads
    for (int i = 0; i < num_threads; ++i) {
        if (pthread_create(&(pool->threads[i]), NULL, thread_do_work, (void*)pool) != 0) {
            thread_pool_destroy(pool, 0);
            return -1;
        }
        pool->started++;
    }

    return 0;
}

int thread_pool_add(thread_pool_t* pool, void (*function)(void*), void* arg)
{
    int next, err = 0;

    if (pool == NULL || function == NULL) {
        return -1;
    }

    if (pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    next = (pool->tail + 1) % pool->task_queue_size;
    do {
        // Are we full?
        if (pool->count == pool->task_queue_size) {
            err = -1;
            break;
        }

        // Are we shutting down?
        if (pool->shutdown) {
            err = -1;
            break;
        }

        // Add task to queue
        pool->tasks[pool->tail].function = function;
        pool->tasks[pool->tail].arg = arg;
        pool->tail = next;
        pool->count += 1;

        // pthread_cond_broadcast
        if (pthread_cond_signal(&(pool->notify)) != 0) {
            err = -1;
            break;
        }
    } while (0);

    if (pthread_mutex_unlock(&pool->lock) != 0) {
        err = -1;
    }

    return err;
}

int thread_pool_destroy(thread_pool_t* pool, int graceful)
{
    if (pool == NULL) {
        return -1;
    }

    if (pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    do {
        if (pool->shutdown) {
            break;
        }
        pool->shutdown = (graceful) ? 1 : 2;

        // Wake up all worker threads
        if ((pthread_cond_broadcast(&(pool->notify)) != 0) || (pthread_mutex_unlock(&(pool->lock)) != 0)) {
            break;
        }

        // Join all worker thread
        for (int i = 0; i < pool->thread_count; i++) {
            if (pthread_join(pool->threads[i], NULL) != 0) {
                break;
            }
        }
    } while (0);

    if (pthread_mutex_unlock(&(pool->lock)) != 0) {
        return -1;
    }

    // Cleanup
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
    free(pool->tasks);
    free(pool->threads);
    pool->started = 0;
    pool->shutdown = 0;

    return 0;
}

void* thread_do_work(void* arg)
{
    thread_pool_t* pool = (thread_pool_t*)arg;
    task_t task;

    while (1) {
        pthread_mutex_lock(&(pool->lock));

        // Wait for notification or shutdown signal
        while ((pool->count == 0) && (!pool->shutdown)) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        if ((pool->shutdown == 1) && (pool->count == 0)) {
            break;
        }

        // Grab our task
        task.function = pool->tasks[pool->head].function;
        task.arg = pool->tasks[pool->head].arg;
        pool->head = (pool->head + 1) % pool->task_queue_size;
        pool->count -= 1;

        // Unlock
        pthread_mutex_unlock(&(pool->lock));

        // Get to work
        (*(task.function))(task.arg);
    }

    pool->started--;

    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return (NULL);
}