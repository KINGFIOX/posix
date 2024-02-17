#ifndef __WORKQ_HXX__
#define __WORKQ_HXX__

#include <pthread.h>

typedef struct workq_ele_tag {
    struct workq_ele_tag* next;
    void* data;
} workq_ele_t;

typedef struct workq_tag {
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    pthread_attr_t attr; // 创建 detached threads
    workq_ele_t *first, *last;
    int valid;
    int quit;
    int parallelism; // 最大线程数量
    int counter; // 当前线程数量
    int idle; // 空闲的线程的数量
    void (*engine)(void* arg);
} workq_t;

#define WORKQ_VALID 0xdec1992

extern int workq_init(
    workq_t* wq,
    int threads,
    void (*engin)(void*));

extern int workq_destroy(workq_t* wq);

extern int workq_add(workq_t* wq, void* data);

#endif
