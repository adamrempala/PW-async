#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stddef.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

struct sigaction action;
sigset_t block_mask;

typedef struct elem que_el;

struct elem {
  struct thread_pool* pool;
  que_el* previous;
  que_el* next;
};

typedef struct runnable {
  void (*function)(void *, size_t);
  void *arg;
  size_t argsz;
} runnable_t;

/***FIFO QUEUE ***/

struct fifo_link;
typedef struct fifo_link fifo_elt;
struct fifo_link {
  runnable_t* runnable;
  fifo_elt* next;
};

typedef struct {
  fifo_elt* first;
  fifo_elt* last;
  size_t fifo_size;
} fifo;

/***THREAD POOL ***/

typedef struct thread_pool {
  pthread_mutex_t integral;
  pthread_mutex_t lock; /* mutex ensuring exclusive access to buffer */
  //pthread_mutex_t lock_2; /* second mutex preventing defer collision with init or destroy*/
  sem_t lock2;
  size_t pool_size;
  pthread_cond_t task_waiting; /* signaled when task is added to fifo */
  sem_t cond2;
  fifo* fifo; /* queue for adding tasks */
  int destroyed; /* is non-zero when pool is going to be destroyed*/
  pthread_t* thread_list; /* list of threads */
  size_t running; /* how many threads have started */
  que_el* link;

} thread_pool_t;

int thread_pool_init(thread_pool_t *pool, size_t pool_size);

void thread_pool_destroy(thread_pool_t *pool);

int defer(thread_pool_t *pool, runnable_t runnable);

#endif
