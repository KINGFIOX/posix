#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "errors.h"

const int CREW_SIZE = 4;

/**
 * @brief 工作队列中的每一项
 * 
 */
typedef struct work_tag {
    struct work_tag* next; /* next work item */
    char* path; /* directory or file */
    char* string; /* search string */
} work_t, *work_p;

typedef struct worker_tag {
    int index;
    pthread_t thread;
    struct crew_tag* crew;
} worker_t, *worker_p;