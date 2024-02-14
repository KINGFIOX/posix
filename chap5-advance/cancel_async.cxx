#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "errors.hxx"

#define SIZE 10

static int matrixA[SIZE][SIZE];
static int matrixB[SIZE][SIZE];
static int matrixC[SIZE][SIZE];

void* thread_routine(void* arg)
{
    int cancel_type;
    int status;

    /* init */
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            matrixA[i][j] = i;
            matrixB[i][j] = j;
        }
    }

    while (1) {
        status = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &cancel_type);
        if (status != 0) {
            err_abort(status, "set cancel type");
        }

        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                matrixC[i][j] = 0;
                for (int k = 0; k < SIZE; k++) {
                    matrixC[i][j] += matrixA[i][k] * matrixB[k][j];
                }
            }
        }

        status = pthread_setcanceltype(cancel_type, &cancel_type);
        if (status != 0) {
            err_abort(status, "set cancel type");
        }

        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                matrixA[i][j] = matrixC[i][j];
            }
        }
    }
}

int main(void)
{
    pthread_t thread_id;

    void* result;

    int status;

    status = pthread_create(&thread_id, NULL, thread_routine, NULL);
    if (status != 0) {
        err_abort(status, "create thread");
    }

    sleep(1);

    status = pthread_cancel(thread_id);
    if (status != 0) {
        err_abort(status, "cancel thread");
    }

    status = pthread_join(thread_id, &result);
    if (status != 0) {
        err_abort(status, "join thread");
    }
    if (result == PTHREAD_CANCELED) {
        printf("thread canceled\n");
    } else {
        printf("thread was not canceled\n");
    }

    return 0;
}