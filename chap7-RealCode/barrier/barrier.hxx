#include <pthread.h>

#ifndef __BARRIER_HXX__
#define __BARRIER_HXX__

typedef struct barrier_tag {
    pthread_mutex_t mutex; // 用来保护 counter
    pthread_cond_t cv;
    int valid; // 设置有效，实际上这个设置为 1 就好了，但是我们可以乱设置一个数字
    int threshold; // 需要多少个线程
    int counter; // current number of threads
    unsigned long cycle; // count cycles
} barrier_t;

#define BARRIER_VALID 0xdbcafe /* 没懂这个 数字是用来干啥的，保存到 valid 中，我的评价是 6 */

#define BARRIER_INITIALIZER(cnt)      \
    {                                 \
        PTHREAD_MUTEX_INITIALIZER,    \
            PTHREAD_COND_INITIALIZER, \
            BARRIER_VALID,            \
            cnt,                      \
            cnt,                      \
            0                         \
    }

/**
 * define barrcntions
 */
extern int barrier_init(barrier_t* barrier, int count);
extern int barrier_destroy(barrier_t* barrier);
extern int barrier_wait(barrier_t* barrier);

#endif //  __BARRIER_HXX__