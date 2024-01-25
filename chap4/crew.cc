#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "errors.h"

const int CREW_SIZE = 4;

/**
 * @brief 每项工作由一个 work_t 结构描述
 *
 */
typedef struct work_tag {
    struct work_tag* next; /* next work item */
    char* path; /* directory or file */
    char* string; /* search string */
} work_t, *work_p;

/**
 * @brief 每个工作组的成员
 *
 */
typedef struct worker_tag {
    int index;
    pthread_t thread;
    struct crew_tag* crew; /* 一个指向 crew 结构的指针 */
} worker_t, *worker_p;

/**
 * @brief 工作组状态
 * 
 */
typedef struct crew_tag {
    int crew_size;
    worker_t crew[CREW_SIZE];
    long work_count; /* 工作数量 */
    work_t *first, *last; /* 任务队列 */
    pthread_mutex_t mutex;
    pthread_cond_t done; /* 等待其他任务完成 */
    pthread_cond_t go; /* 等待接收新工作的条件变量 */
} crew_t, *crew_p;

size_t path_max; /* filepath length */
size_t name_max; /* name length */

/**
 * @brief 
 * 
 * @param arg 传入一个 worker
 * @return void* 
 */
void* worker_routine(void* arg)
{
    worker_p mine = (worker_t*)arg;
    crew_p crew = mine->crew;

    work_p work, new_work; /* 获取对头元素 */

    struct stat filestat;

    int status; /* 错误状态 */

    struct dirent* entry = (struct dirent*)malloc(sizeof(struct dirent) + name_max);
    if (entry == NULL) {
        errno_abort("allocating dirent");
    }

    /* 上锁，保护获取新工作的时候，不会被打断 */
    status = pthread_mutex_lock(&crew->mutex);
    if (status != 0) {
        err_abort(status, "lock crew mutex");
    }
    while (crew->work_count == 0) {
        status = pthread_cond_wait(&crew->go, &crew->mutex);
        if (status != 0) {
            err_abort(status, "wait for go!");
        }
    }
    status = pthread_mutex_unlock(&crew->mutex);
    if (status != 0) {
        err_abort(status, "unlock mutex");
    }
    printf("crew %d starting\n", mine->index);

    while (1) {
        /* work_count == 0 结束；first == NULL 继续 */
        status = pthread_mutex_lock(&crew->mutex);
        if (status != 0) {
            err_abort(status, "lock crew mutex");
        }
        { /* 上锁 */
            printf("crew %d top: first is %#lx, count is %d\n",
                mine->index, crew->first, crew->work_count);

            while (crew->first == NULL) {
                status = pthread_cond_wait(&crew->go, &crew->mutex);
                if (status != 0) {
                    err_abort(status, "wait for work");
                }
            }
            printf("crew %d woke: %#lx, %d\n",
                mine->index, crew->first, crew->work_count);

            /* remove and process a work item */
            work = crew->first;
            crew->first = work->next;
            if (crew->first == NULL) {
                crew->last = NULL;
            }
            printf("crew %d took %#lx, leaves first %#lx, last %#lx\n",
                mine->index, work, crew->first, crew->last);
            /* 解锁 */
        }
        status = pthread_mutex_unlock(&crew->mutex);
        if (status != 0) {
            err_abort(status, "unlock mutex");
        }

        status = lstat(work->path, &filestat);

        if (S_ISLNK(filestat.st_mode)) {
            printf("thread %d: %s is a link, skipping.\n", mine->index, work->path);
        } else if (S_ISDIR(filestat.st_mode)) {
            struct dirent* result;
            DIR* directory = opendir(work->path);
            if (directory == NULL) {
                fprintf(stderr, "unable to open directory %s: %d (%s)\n", work->path, errno, strerror(errno));
                continue;
            }
            while (1) {
                status = readdir_r(directory, entry, &result);
                if (status != 0) {
                    fprintf(stderr, "unable to read directory %s: %d (%s)\n", work->path, status, strerror(status));
                    break;
                }
                if (result == NULL) { /* end of directory while(1) 的出口 */
                    break;
                }
                if (strcmp(entry->d_name, ".") == 0) {
                    continue;
                } else if (strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                new_work = (work_p)malloc(sizeof(work_t));
                if (new_work->path == NULL) {
                    errno_abort("unable to allocate path");
                }
                strcpy(new_work->path, work->path);
                strcat(new_work->path, "/");
                strcat(new_work->path, entry->d_name);
                new_work->string = work->string; /* 查找的子串，指向同一个 const char * */
                new_work->next = NULL;
                status = pthread_mutex_lock(&crew->mutex);
                if (status != 0) {
                    err_abort(status, "lock mutex");
                }
                { /* 上锁 */
                    if (crew->first == NULL) {
                        crew->first = new_work;
                        crew->last = new_work;
                    } else { /* 入队 */
                        crew->last->next = new_work;
                        crew->last = new_work;
                    }
                    crew->work_count++;
                    printf("crew %d: add %#lx, first %#lx, last %#lx, %d\n",
                        mine->index, new_work, crew->first, crew->last, crew->work_count);
                    status = pthread_cond_signal(&crew->go);
                    if (status != 0) {
                        err_abort(status, "cond signal");
                    }
                } /* 解锁 */
                status = pthread_mutex_unlock(&crew->mutex);
                if (status != 0) {
                    err_abort(status, "unlock mutex");
                }
            }
            closedir(directory);
        } else if (S_ISREG(filestat.st_mode)) {
            FILE* search = fopen(work->path, "r");
            char buffer[256], *bufptr, *search_ptr; // bufptr 文件内容指针指向 buffer[0]
            if (search == NULL) {
                fprintf(stderr, "unable to open %s: %d (%s)\n", work->path, errno, strerror(errno));
            } else {
                while (1) {
                    bufptr = fgets(buffer, sizeof(buffer), search);
                    if (bufptr == NULL) {
                        if (feof(search)) {
                            break;
                        }
                        if (ferror(search)) {
                            fprintf(stderr, "unable to read %s: %d (%s)\n",
                                stderr, errno, strerror(errno));
                            break;
                        }
                    }
                    search_ptr = strstr(buffer, work->string);
                    if (search_ptr != NULL) {
                        printf("thread %d found \"%s\" in %s\n", mine->index, work->string, work->path);
                        break;
                    }
                } /* while 有 3 个出口 */
                fclose(search);
            }
        } else {
            fprintf(stderr, "thread %d: %s is type %o (%s)\n",
                mine->index, work->path, filestat.st_mode & S_IFMT,
                (S_ISFIFO(filestat.st_mode) ? "FIFO"
                                            : (S_ISCHR(filestat.st_mode) ? "CHR"
                                                                         : (S_ISBLK(filestat.st_mode) ? "BLK"
                                                                                                      : S_ISSOCK(filestat.st_mode) ? "SOCK"
                                                                                                                                   : "unknown"))));
        }
        free(work->path);
        free(work);
        status = pthread_mutex_lock(&crew->mutex);
        if (status != 0) {
            err_abort(status, "lock crew mutex");
        }
        { /* 上锁 */
            crew->work_count--;
            printf("crew %d decremented work to %d\n", mine->index, crew->work_count);
            if (crew->work_count <= 0) {
                printf("crew thread %d done\n", mine->index);
                status = pthread_cond_broadcast(&crew->done);
                if (status != 0) {
                    err_abort(status, "boardcast done");
                }
                status = pthread_mutex_unlock(&crew->mutex);
                if (status != 0) {
                    err_abort(status, "unlock mutex");
                }
                break;
            }
        } /* 解锁 */
        status = pthread_mutex_unlock(&crew->mutex);
        if (status != 0) {
            err_abort(status, "unlock crew mutex");
        }
    }
    free(entry);
    return NULL;
}