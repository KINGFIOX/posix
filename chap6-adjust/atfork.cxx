#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/_types/_pid_t.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "errors.hxx"

pid_t self_pid; /* 当前进程的 pid */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 在创建子进程前，他将在父进程内被 fork 调用
 *		该函数改变的任何状态（特别是上锁互斥量），将被拷贝到 子进程
 */
void fork_prepare(void)
{
    int status = pthread_mutex_lock(&mutex);
    HANDLE_STATUS("lock in prepare handler");
}

/**
 * @brief 创建子进程后，他将被父进程调用
 *		一个 parent 处理器应该取消在 prepare 处理器中做的处理，以便父进程能正常继续
 */
void fork_parent(void)
{
    int status = pthread_mutex_unlock(&mutex);
    HANDLE_STATUS("unlock in parent handler")
}

void fork_child(void)
{
    self_pid = getpid();
    int status = pthread_mutex_unlock(&mutex);
    HANDLE_STATUS("unlock in child handler");
}

void* thread_routine(void* arg)
{
    int status;

    pid_t child_pid = fork();
    if (child_pid == (pid_t)-1) {
        errno_abort("fork");
    }

    status = pthread_mutex_lock(&mutex);
    HANDLE_STATUS("lock in child");

    status = pthread_mutex_unlock(&mutex);
    HANDLE_STATUS("unlock in child");

    printf("after fork: %d (%d)\n", child_pid, self_pid);

    if (child_pid != 0) {
        if ((pid_t)-1 == waitpid(child_pid, (int*)0, 0)) {
            errno_abort("wait for child");
        }
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    int status;

    pthread_t fork_thread;

    int atfork_flag = 1;
    if (argc > 1) {
        atfork_flag = atoi(argv[1]);
    }
    if (atfork_flag) {
        status = pthread_atfork(fork_prepare, fork_parent, fork_child);
        HANDLE_STATUS("register fork handlers");
    }

    self_pid = getpid();

    status = pthread_mutex_lock(&mutex);
    HANDLE_STATUS("lock mutex");

    status = pthread_create(&fork_thread, NULL, thread_routine, NULL);
    HANDLE_STATUS("create thread");

    sleep(5);

    status = pthread_mutex_unlock(&mutex);
    HANDLE_STATUS("unlock mutex");

    status = pthread_join(fork_thread, NULL);
    HANDLE_STATUS("join thread");

    return 0;
}