#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <math.h>
#include <pthread.h>
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/_pthread/_pthread_mutex_t.h>
#include <sys/_types/_errno_t.h>
#include <unistd.h>

#include "errors.h"

#define CLIENT_THREADS 4

#define REQ_READ 1
#define REQ_WRITE 2
#define REQ_QUIT 3

// 一开始定义 传递个i提示服务器的命令，可以用来要求：读输入、写输入、退出

/**
 * @brief 定义了 拆国内 服务器的每个请求
 *
 */
typedef struct request_tag {
    struct request_tag* next; /* link to next */
    int operation; /* fucntion code */
    int synchronous; /* nonzero if synchronous */
    int done_flag; /* predicate for wait */
    pthread_cond_t done; /* wait for completion */
    char prompt[32];
    char text[128];
} request_t;

/**
 * @brief 提供了 服务器线程环境。包含：同步对象 mutex 和 request，
 * 		服务器是否运行的标志 和 一个上位处理的请求链表
 */
typedef struct tty_server_tag {
    request_t* first;
    request_t* last;
    int running;
    pthread_mutex_t mutex;
    pthread_cond_t request;
} tty_server_t;

/**
 * @brief 控制结构（tty_server） 静态被分配 和 初始化
 *
 */
tty_server_t tty_server = {
    NULL, NULL, 0,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER
};

/**
 * @brief 主程序 和 客户线程 通过 同步对象 协调他们的终止
 *
 */
int client_threads;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clients_done = PTHREAD_COND_INITIALIZER;

/**
 * @brief 服务器 处理 request ，可以多线程
 *
 * @param arg
 * @return void*
 */
void* tty_server_routine(void* arg)
{
    static pthread_mutex_t prompt_mutex = PTHREAD_MUTEX_INITIALIZER;

    int status; /* 错误状态 */

    while (1) {
        status = pthread_mutex_lock(&tty_server.mutex);
        if (status != 0) {
            err_abort(status, "lock server mutex")
        }
        /* 上锁 */

        while (tty_server.first == NULL) {
            status = pthread_cond_wait(&tty_server.request, &tty_server.mutex);
            if (status != 0) {
                err_abort(status, "wait for request");
            }
        }
        /* 从队列中 pop 一个请求 */
        request_t* request = tty_server.first;
        tty_server.first = request->next;

        /* 队列被 pop 完了 */
        if (tty_server.first == NULL) {
            tty_server.last = NULL;
        }

        /* 解锁 */
        status = pthread_mutex_unlock(&tty_server.mutex);
        if (status != 0) {
            err_abort(status, "unlock server mutex")
        }

        int operation = request->operation;
        size_t len; /* 注意：不能在 case 中定义变量 */
        switch (operation) {
        case REQ_QUIT:
            break;
        case REQ_READ:
            if (strlen(request->prompt) > 0) {
                printf("%s\n", request->prompt);
            }
            if (fgets(request->text, 128, stdin) == NULL) {
                request->text[0] = '\0';
            }
            len = strlen(request->text);
            if (len > 0 && request->text[len - 1] == '\n') {
                request->text[len - 1] = '\0';
            }
            break;
        case REQ_WRITE:
            puts(request->text);
            break;
        default:
            break;
        }

        if (request->synchronous) {
            status = pthread_mutex_lock(&tty_server.mutex);
            if (status != 0) {
                err_abort(status, "lock server mutex");
            }
            /* 上锁 */
            request->done_flag = 1;
            status = pthread_cond_signal(&request->done); /* 同步：不停的发 done signal */
            if (status != 0) {
                err_abort(status, "signal server condition");
            }
            /* 解锁 */
            status = pthread_mutex_unlock(&tty_server.mutex);
            if (status != 0) {
                err_abort(status, "unlock server mutex");
            }
        } else {
            free(request);
        }
        if (operation == REQ_QUIT) {
            break;
        }
    }
    return NULL;
}

/**
 * @brief 消息传入到 服务器中
 *
 * @param operation
 * @param sync
 * @param prompt
 * @param string
 */
void tty_server_request(
    int operation,
    int sync,
    const char* prompt,
    char* string)
{
    int status; /* 错误状态，这个会复用的 */

    pthread_t thread; /* 创建服务线程 */

    status = pthread_mutex_lock(&tty_server.mutex);
    if (status != 0) {
        err_abort(status, "lock server mutex");
    }
    /* 上锁 */

    if (!tty_server.running) {
        pthread_attr_t detached_attr;

        /* 后续用于 配置 线程属性 */
        status = pthread_attr_init(&detached_attr);
        if (status != 0) {
            err_abort(status, "init attributes object");
        }
        /* 线程属性 设置为 detachable */
        status = pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);
        if (status != 0) {
            err_abort(status, "set detach state");
        }

        tty_server.running = 1; /* 启动! */
        status = pthread_create(&thread, &detached_attr, tty_server_routine, NULL);
        if (status != 0) {
            err_abort(status, "create server");
        }

        pthread_attr_destroy(&detached_attr); /* 用完了，扔掉 */
    }

    request_t* request = (request_t*)malloc(sizeof(request_t));
    if (request == NULL) {
        errno_abort("allocate request");
    }
    request->next = NULL;
    request->operation = operation;
    request->synchronous = sync;
    if (sync) {
        request->done_flag = 0;
        status = pthread_cond_init(&request->done, NULL);
        if (status != 0) {
            err_abort(status, "init request condition");
        }
    }
    if (prompt != NULL) {
        strncpy(request->prompt, prompt, 32);
    } else {
        request->prompt[0] = '\0';
    }
    if (operation == REQ_WRITE && string != NULL) {
        strncpy(request->text, string, 128);
    } else {
        request->text[0] = '\0';
    }

    if (tty_server.first == NULL) {
        tty_server.first = request;
        tty_server.last = request;
    } else {
        (tty_server.last)->next = request;
        tty_server.last = request;
    }

    status = pthread_cond_signal(&tty_server.request);
    if (status != 0) {
        err_abort(status, "wake server");
    }

    if (sync) {
        while (!request->done_flag) {
            status = pthread_cond_wait(&request->done, &tty_server.mutex);
            if (status != 0) {
                err_abort(status, "wait for sync request");
            }
        }
        if (operation == REQ_READ) {
            if (strlen(request->text) > 0) {
                strcpy(string, request->text);
            } else {
                string[0] = '\0';
            }
        }

        status = pthread_cond_destroy(&request->done);
        if (status != 0) {
            err_abort(status, "destroy request conditon");
        }
        free(request);
    }
    /* 解锁 */
    status = pthread_mutex_unlock(&tty_server.mutex);
    if (status != 0) {
        err_abort(status, "unlock server mutex");
    }
}

/**
 * @brief 客户端发送请求
 *
 * @param arg
 * @return void*
 */
void* client_routine(void* arg)
{
    char prompt[32];
    char string[128];

    char formatted[128];

    int status;

    sprintf(prompt, "Client %p>", arg);
    while (1) {
        tty_server_request(REQ_READ, 1, prompt, string);
        if (strlen(string) == 0) {
            break;
        }
        for (int loops = 0; loops < 4; loops++) {
            sprintf(formatted, "(%p#%d) %s", arg, loops, string);
            tty_server_request(REQ_WRITE, 0, NULL, formatted);
            sleep(1);
        }
    }
    status = pthread_mutex_lock(&client_mutex);
    if (status != 0) {
        err_abort(status, "lock client mutex");
    }
    client_threads--;
    if (client_threads <= 0) {
        status = pthread_cond_signal(&clients_done);
        if (status != 0) {
            err_abort(status, "signal clients done");
        }
    }
    status = pthread_mutex_unlock(&client_mutex);
    if (status != 0) {
        err_abort(status, "unlock lock mutex");
    }
    return NULL;
}

int main(void)
{
    int status; /* 错误状态 */

    pthread_t thread;

    /**
     * @brief 客户端
     *
     */
    client_threads = CLIENT_THREADS;
    for (int count = 0; count < client_threads; count++) {
        status = pthread_create(&thread, NULL, client_routine, (void*)count);
        if (status != 0) {
            err_abort(status, "create client thread");
        }
    }

    status = pthread_mutex_lock(&client_mutex);
    if (status != 0) {
        err_abort(status, "lock client mutex");
    }
    /* 上锁 */
    while (client_threads > 0) {
        status = pthread_cond_wait(&clients_done, &client_mutex);
        if (status != 0) {
            err_abort(status, "wait for clients to finish");
        }
    }
    /* 解锁 */
    status = pthread_mutex_lock(&client_mutex);
    if (status != 0) {
        err_abort(status, "lock client mutex");
    }

    printf("all clients done\n");
    tty_server_request(REQ_QUIT, 1, NULL, NULL);
    return 0;
}