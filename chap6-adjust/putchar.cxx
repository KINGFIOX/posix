#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <sys/_pthread/_pthread_t.h>
#include <unistd.h>

#include "errors.hxx"

void* lock_routine(void* arg)
{

    flockfile(stdout); // 这个应该是 f lock file

    for (char* pointer = (char*)arg; *pointer != '\0'; pointer++) {
        putchar_unlocked(*pointer);
        sleep(1);
    }

    funlockfile(stdout); // f unlock file

    return NULL;
}

/**
 * @brief 虽然不会破坏缓冲区，但是会导致：字符顺序是乱的
 *
 * @param arg
 * @return void*
 */
void* unlock_routine(void* arg)
{
    for (char* pointer = (char*)arg; *pointer != '\0'; pointer++) {
        putchar(*pointer);
        sleep(1);
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    using FUNC = void* (*)(void*);
    FUNC thread_func;

    int status;

    int flock_flag;

    if (argc > 1) {
        flock_flag = atoi(argv[1]);
    }
    if (flock_flag) {
        thread_func = &lock_routine;
    } else {
        thread_func = &unlock_routine;
    }

    pthread_t thread1;
    status = pthread_create(&thread1, NULL, thread_func, (char*)"this is thread 1\n");
    HANDLE_STATUS("create thread");

    pthread_t thread2;
    status = pthread_create(&thread2, NULL, thread_func, (char*)"this is thread 1\n");
    HANDLE_STATUS("create thread");

    pthread_t thread3;
    status = pthread_create(&thread3, NULL, thread_func, (char*)"this is thread 1\n");
    HANDLE_STATUS("create thread");

    pthread_exit(NULL);
}