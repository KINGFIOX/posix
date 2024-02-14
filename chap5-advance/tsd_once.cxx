#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <sys/_pthread/_pthread_once_t.h>
#include <unistd.h>

#include "errors.hxx"

typedef struct tsd_tag {
    pthread_t thread_id;
    char* string;
} tsd_t;

pthread_key_t tsd_key;

pthread_once_t key_once = PTHREAD_ONCE_INIT; /* once 控制变量 */

/**
 * @brief 用来初始化 tsd_key ， 因为 tsd_key 只能被初始化一次
 *
 */
void once_routine(void)
{
    int status;
    printf("initializing key\n");
    status = pthread_key_create(&tsd_key, NULL);
    if (status != 0) {
        err_abort(status, "create key");
    }
}

/**
 * @brief
 *
 * @param arg 指向线程名字字符串
 * @return void*
 */
void* thread_routine(void* arg)
{
    int status;

    status = pthread_once(&key_once, once_routine);
    if (status != 0) {
        err_abort(status, "once init");
    }

    tsd_t* value = (tsd_t*)malloc(sizeof(tsd_t));
    if (value == NULL) {
        errno_abort("allocate key value");
    }

    status = pthread_setspecific(tsd_key, value);
    if (status != 0) {
        err_abort(status, "set tsd");
    }

    printf("%s set tsd value %p\n", (char*)arg, value);

    value->thread_id = pthread_self();
    value->string = (char*)arg;
    value = (tsd_t*)pthread_getspecific(tsd_key);

    /* 这个 string 也就是 arg */
    printf("%s starting...\n", value->string);

    sleep(2);

    value = (tsd_t*)pthread_getspecific(tsd_key);

    printf("%s done...\n", value->string);

    return NULL;
}

int main(int argc, char* argv[])
{

    int status;

    pthread_t thread1;
    char str1[] = "thread 1";
    status = pthread_create(&thread1, NULL, thread_routine, str1);
    if (status != 0) {
        err_abort(status, "create thread1");
    }

    pthread_t thread2;
    char str2[] = "thread 2";
    status = pthread_create(&thread2, NULL, thread_routine, str2);
    if (status != 0) {
        err_abort(status, "create thread2");
    }

    pthread_exit(NULL);
    return 0;
}