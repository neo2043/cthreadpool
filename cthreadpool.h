#ifndef THREADPOOL
#define THREADPOOL

#include <pthread.h>
#include <stdatomic.h>

#define CHECK(cond, ...)                                                                                                                                       \
    do {                                                                                                                                                       \
        if (!(cond)) {                                                                                                                                         \
            fprintf(stderr, "%s:%d CHECK(%s) failed: ", __FILE__, __LINE__, #cond);                                                                            \
            fprintf(stderr, "" __VA_ARGS__);                                                                                                                   \
            fprintf(stderr, "\n");                                                                                                                             \
            exit(1);                                                                                                                                           \
        }                                                                                                                                                      \
    } while (0)

typedef struct threadpool_s threadpool_t;

typedef struct {
    pthread_cond_t jobqueue_wait_cond;
    pthread_mutex_t jobqueue_wait_mutex;
} jobqueue_sync_t;

typedef struct job_t{
    void *arg;
    struct job_t *next;
    void *(*thread_func)(void *arg);
} job_t;

typedef struct {
    int len;
    job_t *rear;
    job_t *front;
    pthread_mutex_t jobqueue_mutex;
    jobqueue_sync_t *jobqueue_sync;
} jobqueue_t;

typedef struct {
    int id;
    pthread_t thread;
    threadpool_t *threadpool;
} thread_t;

struct threadpool_s {
    int thread_num;
    jobqueue_t *jobqueue;
    thread_t *thread_t_arr;
    _Atomic(int) num_threads_alive;
    _Atomic(int) num_threads_working;
    _Atomic(bool) keep_threadpool_alive;
    pthread_cond_t threadpool_thread_idle_cond;
    pthread_mutex_t threadpool_thread_count_mutex;
};

threadpool_t *threadpool_t_init(int thread_num);
void threadpool_add_work(const threadpool_t *threadpool, void *(*worker_func)(void *arg), void *arg);
void threadpool_wait(threadpool_t *threadpool);
void threadpool_destroy(threadpool_t *threadpool);
int get_num_alive_threads(threadpool_t *threadpool);
int get_thread_id(threadpool_t *threadpool);

#endif