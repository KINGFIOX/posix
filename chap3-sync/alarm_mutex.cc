#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <time.h>

#include "errors.h"

typedef struct alarm_tag {
    struct alarm_tag* link;
    int seconds;
    time_t time; /* seconds from epoch */
    char message[64];
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;

alarm_t* alarm_list = NULL;

void* alarm_thread(void* arg)
{
    while (1) {
        int sleep_time;
        if (int status = pthread_mutex_lock(&alarm_mutex); status != 0) {
            err_abort(status, "lock mutex");
        }
        alarm_t* alarm = alarm_list;
        if (alarm == NULL) {
            sleep_time = 1; /* 如果alarm list为空，那么就等待1s，这会让main线程运行，如果链表不为空，那么就pop出链表的第一个元素，看看是否过期 */
        } else {
            alarm_list = alarm->link;
            time_t now = time(NULL);
            if (alarm->time <= now) {
                /* 闹钟已经过期了，那么sleep_time=0 */
                sleep_time = 0;
            } else {
                sleep_time = alarm->time - now;
            }
            printf("[waiting: %d(%d)\"%s\"]\n", alarm->time, sleep_time, alarm->message);
        }

        if (int status = pthread_mutex_lock(&alarm_mutex); status != 0) {
            err_abort(status, "unlock");
        }
        if (sleep_time > 0) {
            sleep(sleep_time);
        } else { /* sleep_time==0，也就是时间到了，那么直接切cpu时间片 */
            sched_yield();
        }

        if (alarm != NULL) {
            printf("(%d) $s\n", alarm->seconds, alarm->message);
            free(alarm);
        }
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    pthread_t thread;
    if (int status = pthread_create(&thread, NULL, alarm_thread, NULL); status != 0) {
        err_abort(status, "create alarm thread");
    }
    while (1) {
        printf("alarm> ");
        char line[128];
        if (fgets(line, sizeof(line), stdin) == NULL) {
            exit(0);
        }
        if (strlen(line) <= 1) {
            continue;
        }
        alarm_t* alarm = (alarm_t*)malloc(sizeof(alarm_t));
        if (alarm == NULL) {
            errno_abort("allocate allrm");
        }
        if (sscanf(line, "%d %64[^\n]", &alarm->seconds, alarm->message) < 2) {
            fprintf(stderr, "bad command\n");
            free(alarm);
        } else {
            if (int status = pthread_mutex_lock(&alarm_mutex); status != 0) {
                err_abort(status, "lock mutex");
            }
            alarm->time = time(NULL) + alarm->seconds;
            alarm_t** last = &alarm_list;
            alarm_t* next = *last;
            /* 链表中，时间是升序排列的 */
            while (next != NULL) {
                if (next->time >= alarm->time) {
                    alarm->link = next;
                    *last = alarm;
                    break;
                }
                last = &next->link;
                next = next->link;
            }
            if (next == NULL) {
                *last = alarm;
                alarm->link = NULL;
            }
            printf("[last: ");
            for (next = alarm_list; next != NULL; next = next->link) {
                printf("%d(%d)[\"%s\"] ", next->time, next->time - time(NULL), next->message);
            }
            printf("]\n");

            if (int status = pthread_mutex_unlock(&alarm_mutex); status != 0) {
                err_abort(status, "unlock mutex");
            }
        }
    }
    return 0;
}