#include <pthread.h>

#include "errors.hxx"
#include "rwlock.hxx"

int rwl_init(rwlock_t* rwl)
{
    int status;

    rwl->r_active = 0;
    rwl->r_wait = 0;
    rwl->w_active = 0;
    rwl->w_wait = 0;

    status = pthread_mutex_init(&rwl->mutex, NULL);
    if (status != 0) {
        return status;
    }

    status = pthread_cond_init(&rwl->read, NULL);
    if (status != 0) {
        pthread_mutex_destroy(&rwl->mutex);
        return status;
    }

    status = pthread_cond_init(&rwl->write, NULL);
    if (status != 0) {
        pthread_cond_destroy(&rwl->read);
        pthread_mutex_destroy(&rwl->mutex);
        return status;
    }

    rwl->valid = RWLOCK_VALID;

    return 0;
}

int rwl_destroy(rwlock_t* rwl)
{
    int status;

    if (rwl->valid != RWLOCK_VALID) {
        return EINVAL;
    }

    status = pthread_mutex_lock(&rwl->mutex);
    if (status != 0) {
        return status;
    }

    if (rwl->r_active > 0 || rwl->w_active > 0) {
        pthread_mutex_unlock(&rwl->mutex);
        return EBUSY;
    }

    if (rwl->r_wait > 0 || rwl->w_wait > 0) {
        pthread_mutex_unlock(&rwl->mutex);
        return EBUSY;
    }

    rwl->valid = 0;
    status = pthread_mutex_unlock(&rwl->mutex);
    if (status != 0) {
        return status;
    }
    status = pthread_mutex_destroy(&rwl->mutex);
    int status1 = pthread_cond_destroy(&rwl->read);
    int status2 = pthread_cond_destroy(&rwl->write);
    return (status != 0 ? status
                        : (status1 != 0 ? status1
                                        : status2));
}

static void rwl_readcleanup(void* arg)
{
    rwlock_t* rwl = (rwlock_t*)arg;
    rwl->r_wait--;
    pthread_mutex_unlock(&rwl->mutex);
}

static void rwl_writecleanup(void* arg)
{
    rwlock_t* rwl = (rwlock_t*)arg;
    rwl->w_wait--;
    pthread_mutex_unlock(&rwl->mutex);
}

int rwl_readlock(rwlock_t* rwl)
{
    int status;

    if (rwl->valid != RWLOCK_VALID) {
        return EINVAL;
    }

    status = pthread_mutex_lock(&rwl->mutex);
    if (status != 0) {
        return status;
    }
    /* 上锁 */
    if (rwl->w_active) { // 如果依然有写线程的话，那么就暂停
        rwl->r_wait++;
        pthread_cleanup_push(rwl_readcleanup, (void*)rwl);
        while (rwl->w_active) {
            status = pthread_cond_wait(&rwl->read, &rwl->mutex);
            if (status != 0) {
                break;
            }
        }
        pthread_cleanup_pop(0); // 弹出的时候，不执行
    }

    if (status == 0) {
        rwl->r_active++; // 这个是用来控制 写线程 等待的，就像上面的 w_active 等待一样
    }
    /* 解锁 */
    pthread_mutex_unlock(&rwl->mutex);
    if (status != 0) {
        return status;
    }
    return status;
}

int rwl_readtrylock(rwlock_t* rwl)
{
    int status1, status2;

    if (rwl->valid != RWLOCK_VALID) {
        return EINVAL;
    }

    status1 = pthread_mutex_lock(&rwl->mutex);
    if (status1 != 0) {
        return status1;
    }
    /* 上锁 */
    if (rwl->w_active) {
        status1 = EBUSY;
    } else {
        rwl->r_active++;
    }
    /* 解锁 */
    status2 = pthread_mutex_unlock(&rwl->mutex);
    if (status2 != 0) {
        return status2;
    }
    return status1;
}

int rwl_readunlock(rwlock_t* rwl)
{
    int status1, status2;
    if (rwl->valid != RWLOCK_VALID) {
        return EINVAL;
    }
    status1 = pthread_mutex_lock(&rwl->mutex);
    if (status1 != 0) {
        return status1;
    }
    /* 上锁 */
    rwl->r_active--;
    if (rwl->r_active == 0 && rwl->w_wait > 0) { // 读了以后就写
        status1 = pthread_cond_signal(&rwl->write);
    }
    /* 解锁 */
    status2 = pthread_mutex_unlock(&rwl->mutex);
    if (status2 != 0) {
        return status2;
    }
    return status1;
}

int rwl_writelock(rwlock_t* rwl)
{
    int status;

    if (rwl->valid != RWLOCK_VALID) {
        return EINVAL;
    }

    status = pthread_mutex_lock(&rwl->mutex);
    if (status != 0) {
        return status;
    }
    /* 上锁 */

    if (rwl->w_active > 0 || rwl->r_active > 0) {
        rwl->w_wait++;
        pthread_cleanup_push(rwl_writecleanup, (void*)rwl);

        while (rwl->w_active > 0 || rwl->r_active > 0) {
            status = pthread_cond_wait(&rwl->write, &rwl->mutex);
            if (status != 0) {
                break;
            }
        }
        pthread_cleanup_pop(0);
        rwl->w_wait--;
    }

    /* 解锁 */
    pthread_mutex_unlock(&rwl->mutex);
    if (status != 0) {
        return status;
    }

    return status;
}

int rwl_writetrylock(rwlock_t* rwl)
{
    int status1, status2;
    if (rwl->valid != RWLOCK_VALID) {
        return EINVAL;
    }

    status1 = pthread_mutex_lock(&rwl->mutex);
    if (status1 != 0) {
        return status1;
    }

    if (rwl->w_active > 0 || rwl->r_active > 0) {
        status1 = EBUSY;
    } else {
        rwl->w_active++;
    }

    status2 = pthread_mutex_unlock(&rwl->mutex);
    if (status2 != 0) {
        return status2;
    }

    return status1;
}

int rwl_writeunlock(rwlock_t* rwl)
{
    int status;

    if (rwl->valid != RWLOCK_VALID) {
        return EINVAL;
    }

    status = pthread_mutex_lock(&rwl->mutex);
    if (status != 0) {
        return status;
    }
    /* 上锁 */

    rwl->w_active = 0;

    if (rwl->r_wait > 0) {
        status = pthread_cond_broadcast(&rwl->read); // 优先：读线程
        if (status != 0) {
            pthread_mutex_unlock(&rwl->mutex);
            return status;
        }
    } else if (rwl->w_wait > 0) {
        status = pthread_cond_signal(&rwl->write);
        if (status != 0) {
            pthread_mutex_unlock(&rwl->mutex);
            return status;
        }
    }

    /* 解锁 */
    status = pthread_mutex_unlock(&rwl->mutex);
    if (status != 0) {
        return status;
    }

    return status;
}