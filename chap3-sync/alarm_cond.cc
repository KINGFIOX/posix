#include <asm-generic/errno.h>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <pthread.h>
#include <time.h>

#include "errors.h"

typedef struct alarm_tag {
    struct alarm_tag* link;
    int seconds;
    time_t time;
    char message[64];
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
alarm_t* alarm_list = NULL;
time_t current_alarm = 0;

/**
 * @brief 链表插入节点
 * 
 * @param alarm 
 */
void alarm_insert(alarm_t* alarm)
{
    alarm_t** last = &alarm_list;
    alarm_t* next = *last;
    while (next != NULL) {
        /* 这种插入方式，就是 linus 说的有品味的插入方式 */
        if (next->time >= alarm->time) {
            alarm->link = next;
            *last = alarm; // 相当于是上一个节点的link
            break;
        }
        last = &next->link;
        next = next->link;
    }
    if (next == NULL) {
        *last = alarm;
        alarm->link = NULL;
    }

    // debug
    printf("[list: ");
    for (next = alarm_list; next != NULL; next = next->link) {
        printf("%d(%d)[\"%s\"] ", next->time, next->time - time(NULL), next->message);
    }
    printf("]\n");

    if (current_alarm == 0 || alarm->time < current_alarm) {
        current_alarm = alarm_list->time; /* 时间到了 */
        if (int status = pthread_cond_signal(&alarm_cond) != 0) {
            err_abort(status, "signale cond");
        }
    }
}

/**
 * @brief 子线程
 * 
 * @param arg 
 * @return void* 
 */
void* alarm_thread(void* arg)
{
    alarm_t* alarm;
    struct timespec cond_time;
    time_t now;
    int status, expired;
    status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0) { // 上锁
        err_abort(status, "lock mutex");
    }
    while (1) {
        current_alarm = 0;
        while (alarm_list == NULL) {
            /* 等待 alarm insert */
            status = pthread_cond_wait(&alarm_cond, &alarm_mutex);
            if (status != 0) {
                err_abort(status, "wait on cond");
            }
        }
        alarm = alarm_list; /* 队头 */
        alarm_list = alarm->link; /* 头删 */
        now = time(NULL);
        expired = 0;
        if (alarm->time > now) {
            /* debug */
            printf("[waiting: %d(%d)\"%s\"]\n", alarm->time, alarm->time - time(NULL), alarm->message);

            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;

            /* 线程等待，直到：当前闹钟到期，或者pthread_cond_timedwait过期 */
            while (current_alarm == alarm->time) {
                /* 超时条件等待过期，那么expired=1，后面在打印闹钟的时候有用 */
                status = pthread_cond_timedwait(&alarm_cond, &alarm_mutex, &cond_time);
                if (status == ETIMEDOUT) {
                    expired = 1;
                    break;
                }
                if (status != 0) {
                    err_abort(status, "cond timewait");
                }
            }

            if (!expired) {
                alarm_insert(alarm);
            }

        } else {
            expired = 1;
        }
        if (expired) { /* 过期了，打印 */
            printf("(%d) %s\n", alarm->seconds, alarm->message);
            free(alarm);
        }
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    int status = 0;
    char line[128];
    alarm_t* alarm;
    pthread_t thread;

    /* 启动子线程 */
    status = pthread_create(&thread, NULL, alarm_thread, NULL);
    if (status != 0) {
        err_abort(status, "create alarm thread");
    }

    while (1) {
        printf("alarm> ");

        if (fgets(line, sizeof(line), stdin) == NULL) {
            exit(0);
        }
        if (strlen(line) <= 1) {
            continue;
        }

        alarm = (alarm_t*)malloc(sizeof(alarm_t));
        if (alarm == NULL) {
            errno_abort("allocate alarm");
        }

        if (sscanf(line, "%d %64[^\n]", &alarm->seconds, alarm->message) < 2) {
            fprintf(stderr, "bad command\n");
            free(alarm);
        } else {
            status = pthread_mutex_lock(&alarm_mutex);
            if (status != 0) {
                err_abort(status, "lock mutex");
            }

            alarm->time = time(NULL) + alarm->seconds;
            alarm_insert(alarm);

            status = pthread_mutex_unlock(&alarm_mutex);
            if (status != 0) {
                err_abort(status, "unlock mutex");
            }
        }
    }
}