#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

#include "errors.hxx"

typedef struct private_tag {
    pthread_t thread_id;
    char* str;
} private_t;

pthread_key_t identity_key;
long identity_key_counter = 0; // 正在使用 key 的线程数
pthread_mutex_t identity_key_mutex = PTHREAD_MUTEX_INITIALIZER; // 用来保护 counter 的

void identity_key_destructor(void* value)
{
    private_t* prvt = (private_t*)value;

    int status;

    printf("thread \"%s\" exiting...\n", prvt->str);
    free(value);

    status = pthread_mutex_lock(&identity_key_mutex);
    HANDLE_STATUS("lock key mutex");
    /* 上锁 */
    identity_key_counter--;
    if (identity_key_counter <= 0) {
        status = pthread_key_delete(identity_key);
        HANDLE_STATUS("delete key");
        printf("key deleted...\n");
    }
    /* 解锁 */
    status = pthread_mutex_unlock(&identity_key_mutex);
    HANDLE_STATUS("unlock key mutex");
}

/**
 * @brief 获取 identity_key 对应的 value （数据）
 *
 * @return void*
 */
void* identity_key_get(void)
{
    void* value;
    int status;

    value = pthread_getspecific(identity_key);

    if (value == NULL) {
        value = malloc(sizeof(private_t));
        if (value == NULL) {
            errno_abort("allocate key value"); // 其实 errno 就是一个 tsd
        }
        status = pthread_setspecific(identity_key, (void*)value);
        HANDLE_STATUS("set tsd");
    }
    return value;
}

void* thread_routine(void* arg)
{
    private_t* value = (private_t*)identity_key_get();
    value->thread_id = pthread_self();
    value->str = (char*)arg;
    printf("thread \"%s\" starting...\n", value->str);

    sleep(2);

    return NULL;
}

int main(int argc, char* argv[])
{
    int status;

    status = pthread_key_create(&identity_key, identity_key_destructor);
    HANDLE_STATUS("create key");
    identity_key_counter = 3;

    private_t* value = (private_t*)identity_key_get();
    value->thread_id = pthread_self();
    value->str = "main thread";

    pthread_t thread1;
    status = pthread_create(&thread1, NULL, thread_routine, (void*)"thread 1");
    HANDLE_STATUS("create thread 1")

    pthread_t thread2;
    status = pthread_create(&thread2, NULL, thread_routine, (void*)"thread 2");
    HANDLE_STATUS("create thread 2")

    pthread_exit(NULL);
}