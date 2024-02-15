#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <pthread/sched.h>
#include <signal.h>
#include <sys/_pthread/_pthread_mutex_t.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/_types/_sigset_t.h>
#include <sys/signal.h>
#include <unistd.h>

#include "errors.hxx"

#define THREAD_COUNT 20
#define ITERATORS 40000 /* 定义了目标线程 循环次数 */

unsigned long thread_count = THREAD_COUNT;

unsigned long iteratorions = ITERATORS;

pthread_mutex_t the_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER; /* 用来保护 arr 的 */

volatile sig_atomic_t sentinel = 0; /* sentinel 哨兵 */
/* 用于同步：信号处理函数 与 其他线程 */

pthread_once_t once = PTHREAD_ONCE_INIT;

pthread_t* arr = NULL;

pthread_t null_pthread = { 0 };

int bottom = 0;

int inited = 0;

/**
 * @brief 这是 sigusr1 的处理函数
 *
 * @param sig
 */
void suspend_signal_handler(int sig)
{
    sigset_t signal_set;

    /* 只能接收 SIGUSR2 信号 */
    sigfillset(&signal_set);
    sigdelset(&signal_set, SIGUSR2);
    sentinel = 1;
    sigsuspend(&signal_set);

    return;
}

/**
 * @brief 这个函数啥也没干。会在接受到 sigusr2 后让 sigsuspend 返回
 *
 * @param sig
 */
void resume_signal_handler(int sig) { return; }

/**
 * @brief 信号初始化 处理函数
 *
 */
void suspend_init_routine(void)
{
    int status;

    bottom = 10;

    arr = (pthread_t*)calloc(bottom, sizeof(pthread_t));

    struct sigaction sigusr1;
    sigusr1.sa_flags = 0;
    sigusr1.sa_handler = suspend_signal_handler;
    sigemptyset(&sigusr1.sa_mask);
    sigaddset(&sigusr1.sa_mask, SIGUSR2);

    struct sigaction sigusr2;
    sigusr2.sa_flags = 0;
    sigusr2.sa_handler = resume_signal_handler;

    status = sigaction(SIGUSR1, &sigusr1, NULL);
    HANDLE_STATUS("installing suspend handler");

    sigemptyset(&sigusr2.sa_mask);
    status = sigaction(SIGUSR2, &sigusr2, NULL);
    HANDLE_STATUS("installing resume handler");

    inited = 1;
    return;
}

/**
 * @brief 挂起指定的 线程
 *
 * @param target_thread
 * @return int 返回状态 status，相当于错误 向上一级抛出
 */
int thd_suspend(pthread_t target_thread)
{
    int status;

    /* 确保 只被初始化一次 */
    status = pthread_once(&once, suspend_init_routine);
    if (status != 0) {
        return status;
    }

    status = pthread_mutex_lock(&mut);
    if (status != 0) {
        return status;
    }
    /* 加锁，因为要访问 全局的 arr */

    int i = 0;
    while (i < bottom) {
        if (pthread_equal(arr[i++], target_thread)) {
            status = pthread_mutex_unlock(&mut);
            return status;
        }
    }

    /* 如果走到这里，说明：target_thread 不在 arr 中 */

    i = 0;
    while (i < bottom && arr[i] != 0) {
        i++;
    }

    if (i == bottom) {
        /* 走到这里，说明了：arr 无空闲空间 ---> 扩容 */
        arr = (pthread_t*)realloc(arr, (++bottom * sizeof(pthread_t)));
        if (arr == NULL) {
            pthread_mutex_unlock(&mut);
            return errno;
        }

        arr[bottom] = null_pthread; /* 并将 arr 末尾的 thread 赋值为 null_pthread */
    }

    sentinel = 0; /* 用于检查 目标线程何时被推迟执行 */
    status = pthread_kill(target_thread, SIGUSR1);
    if (status != 0) {
        pthread_mutex_unlock(&mut);
        return status;
    }

    while (sentinel == 0) {
        sched_yield(); /* 让出 cpu 执行权 */
    }

    arr[i] = target_thread;

    status = pthread_mutex_unlock(&mut);
    return status;
}

int thd_continue(pthread_t target_thread)
{
    int status;

    status = pthread_once(&once, suspend_init_routine);
    if (status != 0) {
        return status;
    }

    status = pthread_mutex_lock(&mut);
    if (status != 0) {
        return status;
    }

    int i = 0;
    while (i < bottom && !pthread_equal(arr[i], target_thread)) {
        i++;
    }

    if (i == bottom) {
        /* 没找到指定的线程 ---> 解锁、返回 */
        pthread_mutex_unlock(&mut);
        return 0;
    }

    status = pthread_kill(target_thread, SIGUSR2); /* 发送恢复信号 */
    if (status != 0) {
        pthread_mutex_unlock(&mut);
        return status;
    }

    arr[i] = 0; /* 当前进程恢复了，clear arr elem */
    status = pthread_mutex_unlock(&mut);

    return status;
}

static void* thread_routine(void* arg)
{
    long number = (long)arg;

    int status;

    char buffer[128];

    for (int i = 1; i < iteratorions; i++) {
        if (i % 2000 == 0) {
            sprintf(buffer, "thread %02d: %d\n", number, i);

            write(1, buffer, strlen(buffer)); /* fd=1 ---> stdout */
        }

        sched_yield();
    }

    return (void*)0;
}

int main(int argc, char* argv[])
{
    int status;

    /* 设置 线程属性 */
    pthread_attr_t detach;
    status = pthread_attr_init(&detach);
    HANDLE_STATUS("init attributes object");
    status = pthread_attr_setdetachstate(&detach, PTHREAD_CREATE_DETACHED);
    HANDLE_STATUS("set create-detached");

    /* 创建线程 */
    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        status = pthread_create(&threads[i], &detach, thread_routine, (void*)i);
        HANDLE_STATUS("create thread");
    }

    status = pthread_attr_destroy(&detach);
    HANDLE_STATUS("destroy thread attribution")

    printf("sleeping ...\n"); /* 休眠两秒，让 线程们 达到稳定状态 */
    sleep(2);

    /* 先 continue 一次，相当于是 复位操作，反正 sigusr2 的处理函数里面没啥东西 */
    for (int i = THREAD_COUNT / 2; i < THREAD_COUNT; i++) {
        printf("continuing thread %d.\n", i);
        status = thd_continue(threads[i]);
        HANDLE_STATUS("continue thread");
    }

    /* 挂起 后面一半的 线程 */
    for (int i = THREAD_COUNT / 2; i < THREAD_COUNT; i++) {
        printf("suspending thread %d.\n", i);
        status = thd_suspend(threads[i]);
        HANDLE_STATUS("suspend thread");
    }

    printf("sleeping ...\n");
    sleep(2);

    /* 恢复线程 */
    for (int i = THREAD_COUNT / 2; i < THREAD_COUNT; i++) {
        printf("continuing thread %d.\n", i);
        status = thd_continue(threads[i]);
        HANDLE_STATUS("continue thread");
    }

    pthread_exit(NULL);
}