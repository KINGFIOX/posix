/**
 * @file cond_dynamic.cc
 * @author your name (you@domain.com)
 * @brief 动态的初始化 cond, mutex 而不是 PTHREAD_MUTEX_INITIALIZER
 * @version 0.1
 * @date 2024-01-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <cstdlib>
#include <pthread.h>

#include "errors.h"

typedef struct my_struct_tag {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int value;
} my_struct_t;

int main(int argc, char* argv[])
{
    my_struct_t* data = new my_struct_t;
    if (data == NULL) {
        errno_abort("allocate structure");
    }
    if (int status = pthread_mutex_init(&data->mutex, NULL) != 0) {
        err_abort(status, "init mutex");
    }
    if (int status = pthread_cond_init(&data->cond, NULL) != 0) {
        err_abort(status, "init conda");
    }
    if (int status = pthread_cond_destroy(&data->cond) != 0) {
        err_abort(status, "cond destroy");
    }
    if (int status = pthread_mutex_destroy(&data->mutex) != 0) {
        err_abort(status, "mutex destroy");
    }
    delete data;
    return 0;
}
