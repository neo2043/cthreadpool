#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cthreadpool.h"

/* ======================== STATIC FUNCTIONS DEFINED ========================= */

// THREAD
static void thread_t_arr_init(threadpool_t *threadpool, int thread_num);
static void *thread_do(void *arg);

// jobqueue
static jobqueue_t *jobqueue_t_init(void);
static void jobqueue_t_deinit(jobqueue_t *jobqueue);
static job_t *jobqueue_pull(jobqueue_t *jobqueue);
void jobqueue_push(jobqueue_t *jobqueue, job_t *newjob);
static void jobqueue_clear(jobqueue_t *jobqueue);

// sync
static jobqueue_sync_t *jobqueue_sync_t_init(void);
static void jobqueue_sync_t_deinit(jobqueue_sync_t *jobqueue_sync);
static void jobqueue_sync_post(jobqueue_sync_t *jobqueue_sync);
static void jobqueue_sync_post_all(jobqueue_sync_t *jobqueue_sync);
static void jobqueue_sync_wait(jobqueue_sync_t *jobqueue_sync);

/* ======================== THREADPOOL ========================= */

// threadpool init
threadpool_t *threadpool_t_init(int thread_num) {
    threadpool_t *threadpool = malloc(sizeof(threadpool_t));
    threadpool->thread_num = thread_num;
    CHECK(threadpool != NULL, "threadpool calloc");
    CHECK(pthread_mutex_init(&threadpool->threadpool_thread_count_mutex, NULL) == 0, "threadpool_t mutex init");
    CHECK(pthread_cond_init(&threadpool->threadpool_thread_idle_cond, NULL) == 0, "threadpool_t cond init");

    atomic_init(&threadpool->keep_threadpool_alive, true);
    atomic_init(&threadpool->num_threads_alive, 0);
    atomic_init(&threadpool->num_threads_working, 0);

    threadpool->jobqueue = jobqueue_t_init();

    threadpool->thread_t_arr = calloc(thread_num, sizeof(thread_t));
    CHECK(threadpool->thread_t_arr != NULL, "threadpool_t thread init");
    thread_t_arr_init(threadpool, thread_num);

    return threadpool;
}

// threadpool add work
void threadpool_add_work(const threadpool_t *threadpool, void *(*worker_func)(void *arg), void *arg) {
    job_t *newjob = malloc(sizeof(job_t));
    CHECK(newjob != NULL, "newjob malloc");
    newjob->thread_func = worker_func;
    newjob->arg = arg;
    newjob->next = NULL;
    jobqueue_push(threadpool->jobqueue, newjob);
}

// threadpool wait
void threadpool_wait(threadpool_t *threadpool) {
    pthread_mutex_lock(&threadpool->threadpool_thread_count_mutex);
    while ((pthread_mutex_lock(&threadpool->jobqueue->jobqueue_mutex), threadpool->jobqueue->len) || atomic_load(&threadpool->num_threads_working)) {
		pthread_mutex_unlock(&threadpool->jobqueue->jobqueue_mutex);
        pthread_cond_wait(&threadpool->threadpool_thread_idle_cond, &threadpool->threadpool_thread_count_mutex);
    }
	pthread_mutex_unlock(&threadpool->jobqueue->jobqueue_mutex);
    pthread_mutex_unlock(&threadpool->threadpool_thread_count_mutex);
}

// threadpool destroy
void threadpool_destroy(threadpool_t *threadpool) {
    if (threadpool == NULL) { return; }
    atomic_store(&threadpool->keep_threadpool_alive, 0);
    while (atomic_load(&threadpool->num_threads_alive)) {
        jobqueue_sync_post_all(threadpool->jobqueue->jobqueue_sync);
    }
    jobqueue_t_deinit(threadpool->jobqueue);
    free(threadpool->thread_t_arr);
    free(threadpool);
}

// get alive thread
int get_num_alive_threads(threadpool_t *threadpool) { return atomic_load(&threadpool->num_threads_alive); }

/* ======================== THREAD ========================= */

// thread init
static void thread_t_arr_init(threadpool_t *threadpool, int num_thread) {
    for (int i = 0; i < num_thread; i++) {
        threadpool->thread_t_arr[i].threadpool = threadpool;
        threadpool->thread_t_arr[i].id = i;
        CHECK(pthread_create(&threadpool->thread_t_arr[i].thread, NULL, thread_do, &threadpool->thread_t_arr[i]) == 0, "new thread create");
        CHECK(pthread_detach(threadpool->thread_t_arr[i].thread) == 0, "new thread detach");
    }
}

// thread_do
static void *thread_do(void *arg) {
    thread_t *thread = (thread_t *)arg;

    atomic_fetch_add(&thread->threadpool->num_threads_alive, 1);

    while (atomic_load(&thread->threadpool->keep_threadpool_alive)) {
        job_t *job;
        while (atomic_load(&thread->threadpool->keep_threadpool_alive) && (job = jobqueue_pull(thread->threadpool->jobqueue)) == NULL) {
            jobqueue_sync_wait(thread->threadpool->jobqueue->jobqueue_sync);
        }

        if (atomic_load(&thread->threadpool->keep_threadpool_alive)) {
            atomic_fetch_add(&thread->threadpool->num_threads_working, 1);

            job->thread_func(job->arg);
            free(job);

            atomic_fetch_sub(&thread->threadpool->num_threads_working, 1);
            if (!atomic_load(&thread->threadpool->num_threads_working)) { pthread_cond_signal(&thread->threadpool->threadpool_thread_idle_cond); }
        }
    }
    atomic_fetch_sub(&thread->threadpool->num_threads_alive, 1);
    return NULL;
}

int get_thread_id(threadpool_t *threadpool) {
    pthread_t curr_thread = pthread_self();
    for (int i = 0; i < threadpool->thread_num; i++) {
        if (pthread_equal(threadpool->thread_t_arr[i].thread, curr_thread)) { return i; }
    }
    return -1;
}

/* ======================== JOBQUEUE ========================= */

// job queue init
static jobqueue_t *jobqueue_t_init(void) {
    jobqueue_t *jobqueue = calloc(1, sizeof(jobqueue_t));
    CHECK(jobqueue != NULL, "jobqueue calloc");
    CHECK(pthread_mutex_init(&jobqueue->jobqueue_mutex, NULL) == 0, "jobqueue_t mutex init");

    jobqueue->jobqueue_sync = jobqueue_sync_t_init();
    return jobqueue;
}

// jobqueue deinit
static void jobqueue_t_deinit(jobqueue_t *jobqueue) {
    jobqueue_sync_t_deinit(jobqueue->jobqueue_sync);
    jobqueue_clear(jobqueue);
    CHECK(pthread_mutex_destroy(&jobqueue->jobqueue_mutex) == 0, "jobqueu_t mutex deinit");
    free(jobqueue);
}

// jobqueue pull
static job_t *jobqueue_pull(jobqueue_t *jobqueue) {
    pthread_mutex_lock(&jobqueue->jobqueue_mutex);
    job_t *job = jobqueue->front;

    switch (jobqueue->len) {
        case 0:
            // pthread_mutex_unlock(&jobqueue->jobqueue_mutex);
            // return NULL;
            break;
        case 1:
            jobqueue->front = NULL;
            jobqueue->rear = NULL;
            jobqueue->len = 0;
            break;
        default:
            jobqueue->front = (job_t *)job->next;
            jobqueue->len--;
            jobqueue_sync_post_all(jobqueue->jobqueue_sync);
    }
    pthread_mutex_unlock(&jobqueue->jobqueue_mutex);
    return job;
}

// jobqueue push
void jobqueue_push(jobqueue_t *jobqueue, job_t *newjob) {
    pthread_mutex_lock(&jobqueue->jobqueue_mutex);
    switch (jobqueue->len) {
        case 0:
            jobqueue->front = newjob;
            jobqueue->rear = newjob;
            break;
        default:
            jobqueue->rear->next = (struct job_t *)newjob;
            jobqueue->rear = newjob;
    }
    jobqueue->len++;
    jobqueue_sync_post_all(jobqueue->jobqueue_sync);
    pthread_mutex_unlock(&jobqueue->jobqueue_mutex);
}

// jobqueue clear
static void jobqueue_clear(jobqueue_t *jobqueue) {
    while (jobqueue->len) {
        free(jobqueue_pull(jobqueue));
    }
    jobqueue->front = NULL;
    jobqueue->rear = NULL;
    jobqueue->len = 0;
}

/* ======================== SYNCHRONISATION ========================= */

// jobqueue wait init
static jobqueue_sync_t *jobqueue_sync_t_init(void) {
    jobqueue_sync_t *jobqueue_sync = malloc(sizeof(jobqueue_sync_t));
    CHECK(jobqueue_sync != NULL, "jobqueue sync malloc");
    CHECK(pthread_cond_init(&jobqueue_sync->jobqueue_wait_cond, NULL) == 0, "job_queue_wait_sync_t cond init");
    CHECK(pthread_mutex_init(&jobqueue_sync->jobqueue_wait_mutex, NULL) == 0, "job_queue_wait_sync_t mutex init");
    return jobqueue_sync;
}

// jobqueue wait deinit
static void jobqueue_sync_t_deinit(jobqueue_sync_t *jobqueue_sync) {
    CHECK(pthread_cond_destroy(&jobqueue_sync->jobqueue_wait_cond) == 0, "job_queue_wait_sync_t cond deinit");
    CHECK(pthread_mutex_destroy(&jobqueue_sync->jobqueue_wait_mutex) == 0, "job_queue_wait_sync_t mutex deinit");
    free(jobqueue_sync);
}

static void jobqueue_sync_post(jobqueue_sync_t *jobqueue_sync) {
    pthread_mutex_lock(&jobqueue_sync->jobqueue_wait_mutex);
    pthread_cond_signal(&jobqueue_sync->jobqueue_wait_cond);
    pthread_mutex_unlock(&jobqueue_sync->jobqueue_wait_mutex);
}

static void jobqueue_sync_post_all(jobqueue_sync_t *jobqueue_sync) {
    pthread_mutex_lock(&jobqueue_sync->jobqueue_wait_mutex);
    pthread_cond_broadcast(&jobqueue_sync->jobqueue_wait_cond);
    pthread_mutex_unlock(&jobqueue_sync->jobqueue_wait_mutex);
}

static void jobqueue_sync_wait(jobqueue_sync_t *jobqueue_sync) {
    pthread_mutex_lock(&jobqueue_sync->jobqueue_wait_mutex);
    pthread_cond_wait(&jobqueue_sync->jobqueue_wait_cond, &jobqueue_sync->jobqueue_wait_mutex);
    pthread_mutex_unlock(&jobqueue_sync->jobqueue_wait_mutex);
}
