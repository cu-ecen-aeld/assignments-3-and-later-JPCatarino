#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

// sleeps for msec milliseconds
// source: https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds
int msleep(long msec)
{
    struct timespec ts;
    int ret;

    if (msec < 0)
    {
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    ret = nanosleep(&ts, &ts);

    return ret;
}

void* threadfunc(void* thread_param)
{
    int ret;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    ret = msleep(thread_func_args->wait_to_obtain_ms);
    if (ret != 0) {
        ERROR_LOG("thread couldn't sleep");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    ret = pthread_mutex_lock(thread_func_args->mutex);
    if (ret != 0) {
        ERROR_LOG("lock failed with err %d", ret);
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    ret = msleep(thread_func_args->wait_to_release_ms);
    if (ret != 0) {
        ERROR_LOG("thread couldn't sleep");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    ret = pthread_mutex_unlock(thread_func_args->mutex);
    if (ret != 0) {
        ERROR_LOG("unlock failed with err %d", ret);
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    int ret;

    struct thread_data* thread_func_args = (struct thread_data*) malloc(sizeof(struct thread_data));
    thread_func_args->mutex = mutex;
    thread_func_args->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_func_args->wait_to_release_ms = wait_to_release_ms;
    thread_func_args->thread_complete_success = false;

    ret = pthread_create(thread, NULL, threadfunc, thread_func_args);
    if (ret != 0) { 
        ERROR_LOG("failed to create thread with err %d", ret);
        return false;
    }

    DEBUG_LOG("thread created");
    return true;
}

