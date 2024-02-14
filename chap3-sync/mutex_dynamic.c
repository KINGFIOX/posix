#include <pthread.h>
#include <stdlib.h>

#include "errors.h"

typedef struct my_struct_tag {
    pthread_mutex_t mutex;
    int value;
} my_struct_t;

int main(int argc, char* argv[])
{
    my_struct_t* data = malloc(sizeof(my_struct_t));

    if (data == NULL) {
        errno_abort("allocate structure");
    }

    int status = pthread_mutex_init(&data->mutex, NULL);

    if (status != 0) {
        err_abort(status, "init mutex");
    }
    status = pthread_mutex_destroy(&data->mutex);
    if (status != 0) {
        err_abort(status, "destroy mutex")
    }

    (void)free(data);

    return status;
}
