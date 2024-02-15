/**
 * @file flock.cxx
 * @author your name (you@domain.com)
 * @brief stdio
 * @version 0.1
 * @date 2024-02-15
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

#include "errors.hxx"

/**
 * @brief 从标准输入中，读入字符串，并返回
 *
 * @param arg
 * @return void*
 */
void* prompt_routine(void* arg)
{
    char* prompt = (char*)arg;

    size_t len;

    char* str = (char*)malloc(128);
    if (str == NULL) {
        errno_abort("alloc string");
    }

    /* 文件锁定 */
    flockfile(stdin);
    flockfile(stdout);

    printf(prompt);
    if (fgets(str, 128, stdin) == NULL) {
        str[0] = '\0';
    } else {
        len = strlen(str);
        if (len > 0 && str[len - 1] == '\n') {
            str[len - 1] = '\0';
        }
    }

    /* 文件解锁，与 文件锁定 是相反顺序 */
    funlockfile(stdout);
    funlockfile(stdin);

    return str;
}

int main(int argc, char* argv[])
{
    int status;

    void* ret;

    pthread_t thread1;
    status = pthread_create(&thread1, NULL, prompt_routine, (char*)"Thread 1> ");
    HANDLE_STATUS("create thread");

    pthread_t thread2;
    status = pthread_create(&thread2, NULL, prompt_routine, (char*)"Thread 2> ");
    HANDLE_STATUS("create thread");

    pthread_t thread3;
    status = pthread_create(&thread3, NULL, prompt_routine, (char*)"Thread 3> ");
    HANDLE_STATUS("create thread");

    status = pthread_join(thread1, &ret);
    HANDLE_STATUS("join thread");
    printf("thread 1: \"%s\"\n", (char*)ret);
    free(ret);

    status = pthread_join(thread2, &ret);
    HANDLE_STATUS("join thread");
    printf("thread 2: \"%s\"\n", (char*)ret);
    free(ret);

    status = pthread_join(thread3, &ret);
    HANDLE_STATUS("join thread");
    printf("thread 3: \"%s\"\n", (char*)ret);
    free(ret);

    return 0;
}