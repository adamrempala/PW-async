#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


//sem_t for_queue;
int initializer = 0;

typedef struct elem que_el;
static struct {
  que_el* first;
  que_el* last;
  int size;
} queue;

int queue_push(struct thread_pool* pool) {
  que_el* x = (que_el*)malloc(sizeof(que_el));

  if (x == NULL)
    return -1;

  queue.size++;

  x->pool = pool;
  pool->link = x;

  if (queue.size == 1) {
    x->previous = NULL;
    x->next = NULL;
    queue.first = x;
    queue.last = x;
  }
  else {
    x->next = queue.first;
    queue.first->previous = x;
    queue.first = x;
  }
  return 0;
}

void queue_clear(int sig __attribute__((unused))) {
  while(queue.first != NULL) {
    thread_pool_destroy(queue.first->pool);
    que_el* next = queue.first->next;
    free(queue.first);
    queue.first = next;
  }
}

void queue_pop (que_el* el) {
  if (queue.size == 1) {
    free(el);
    queue.first = NULL;
    queue.last = NULL;
  }
  else if (el->previous == NULL) {
    queue.first = el->next;
    queue.first->previous = NULL;
    free(el);
  }
  else if (el->next == NULL) {
    queue.last = el->previous;
    queue.last->next = NULL;
    free(el);
  }
  queue.size--;
}

/*** FIFO FUNCTIONS ***/

size_t fifo_size(fifo* fifo) {
  return fifo->fifo_size;
}

int fifo_push(fifo* fifo, runnable_t* runnable) {
  /* allocating memory for new structures */
  fifo_elt* x = (fifo_elt*)malloc(sizeof(fifo_elt));
  if (x == NULL) return -1;
  runnable_t* new_r = (runnable_t*)malloc(sizeof(runnable_t));
  if (new_r == NULL) {
    free(x);
    return -1;
  }


  x->runnable = new_r;
  new_r->function = runnable->function;
  new_r->arg = runnable->arg;
  new_r->argsz = runnable->argsz;
  x->next = NULL;

  if (fifo_size(fifo) == 0) {
    fifo->first = x;
    fifo->last = x;
  }
  else {
    fifo->last->next = x;
    fifo->last = x;

  }
  fifo->fifo_size++;
  return 0;
}

runnable_t* fifo_pop(fifo* fifo) {
  fifo_elt* x = fifo->first->next;
  runnable_t* y = fifo->first->runnable;
  free(fifo->first);
  fifo->first = x;
  fifo->fifo_size--;
  return y;
}

void fifo_clear(fifo* fifo) {
  if (fifo != NULL) {
    fifo_elt* x = fifo->first;
    while(x != NULL) {
      fifo_elt* y = x->next;
      free(x->runnable);
      free(x);
      x = y;
    }
    free(fifo);
  }
}


/*** THREAD POOL FUNCTIONS***/

/*** deallocates all memory ***/
void thread_pool_clear(thread_pool_t *pool) {

  int err;

  free(pool->thread_list);
  fifo_clear(pool->fifo);
  pool->thread_list = NULL;
  pool->fifo = NULL;
  queue_pop(pool->link);
  pool->link = NULL;

  if ((err = pthread_cond_destroy(&(pool->task_waiting))) != 0)
    exit(err);

  if ((err = pthread_mutex_destroy(&(pool->lock))) != 0) {
    exit(err);
  }

  if ((err = sem_destroy(&(pool->lock2))) != 0) {
    exit(err);
  }

}

/* stops all started threads */
void thread_pool_stop_started(thread_pool_t *pool) {
  int err;
  pool->destroyed = 1;

  if ((err = pthread_cond_broadcast(&(pool->task_waiting))) != 0) {
    exit(err);
  }

  pthread_t* list = pool->thread_list;
  size_t elct = pool->running;
  pool->thread_list = NULL;

  if ((err = pthread_mutex_unlock(&(pool->lock)) != 0)) {
    thread_pool_clear(pool);
    exit(err);
  }

  for(size_t i = 0; i < elct; i++) {
    if((err = pthread_join(list[i], NULL)) != 0) {
      exit(err);
    }

  }

  free(list);
}

/*** loop for threads working ***/
static void *thread_pool_work(void *data) {
  thread_pool_t* pool = (thread_pool_t*) data;
  runnable_t* served;
  int err;
  while (true) {
    // we are the only thread having access to variables
    if ((err = pthread_mutex_lock(&(pool->lock))) != 0) {
      thread_pool_clear(pool);
      exit(err);
    }

    // we can go further if we have a task or pool is to be destroyed
    while (fifo_size(pool->fifo) == 0 && pool->destroyed == 0) {
      if ((err = pthread_cond_wait(&(pool->task_waiting), &(pool->lock))) != 0) {
        exit(err);
      }

    }

    // if pool is to be destroyed, firstly we have to do all tasks
    if (fifo_size(pool->fifo) == 0 && pool->destroyed == 1) {
      if ((err = pthread_mutex_unlock(&(pool->lock))) != 0) {
        exit(err);
      }

      pthread_exit(0);
    }


    served = fifo_pop(pool->fifo);
    void (*function)(void *, size_t) = served->function;
    void *arg = served->arg;
    size_t argsz = served->argsz;
    free(served);

    if ((err = pthread_mutex_unlock(&(pool->lock))) != 0) {
      thread_pool_clear(pool);
      exit(err);
    }

    if (sigprocmask(SIG_BLOCK, &block_mask, 0) == -1)
      exit(-1);
    (*function)(arg, argsz);
  }
}


int thread_pool_init(thread_pool_t *pool, size_t num_threads) {
  int err;

  if (initializer == 0) {
    initializer = 1;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
    queue.size = 0;

    action.sa_handler = queue_clear;
    action.sa_mask = block_mask;
    if (sigaction (SIGINT, &action, 0) == -1)
      exit(-1);
  }

  if (sigprocmask(SIG_BLOCK, &block_mask, 0) == -1)
    exit(-1);

  if ((sem_init(&(pool->lock2), 0, 0), NULL) != 0) {
    return -1;
  }

  if (pthread_mutex_init(&(pool->lock), NULL) != 0) {
    if ((err = sem_destroy(&(pool->lock2))) != 0)
      exit(err);
    return -1;
  }

  if ((err = pthread_cond_init(&pool->task_waiting, NULL)) != 0) {
    int err2;
    if ((err2 = sem_destroy(&(pool->lock2))) != 0)
      exit(err2);
    if ((err2 = pthread_mutex_destroy(&(pool->lock))) != 0)
      exit(err2);
    exit(err);
  }

  pool->pool_size = num_threads;
  pool->destroyed = 0;
  pool->running = 0;

  pool->thread_list = (pthread_t*)malloc(sizeof(pthread_t) * pool->pool_size);
  pool->fifo = (fifo*)malloc(sizeof(fifo));

  if (pool->thread_list == NULL || pool->fifo == NULL || queue_push(pool) != 0) {
    thread_pool_clear(pool);
    return -1;
  }

  pool->fifo->fifo_size = 0;

  for (size_t i = 0; i < pool->pool_size; i++) {
    if((pthread_create(&(pool->thread_list[i]),
      NULL, thread_pool_work, (void*)pool)) != 0) {
      if ((err = pthread_mutex_lock(&(pool->lock))) != 0) {
        thread_pool_clear(pool);
        exit(err);
      }
      thread_pool_stop_started(pool);
      thread_pool_clear(pool);
      return -1;
    }
    pool->running++;
  }

  if ((err = sem_post(&(pool->lock2))) != 0)
    exit(err);

  if (sigprocmask(SIG_UNBLOCK, &block_mask, 0) == -1)
    exit(-1);

  return 0;
}

void thread_pool_destroy(struct thread_pool *pool) {
  int err;

  if (sigprocmask(SIG_BLOCK, &block_mask, 0) == -1)
    exit(-1);

  if ((err = sem_wait(&(pool->lock2))) != 0)
    exit(err);

  if ((err = pthread_mutex_lock(&(pool->lock))) != 0) {
    thread_pool_clear(pool);
    exit(err);
  }

  if (pool->destroyed == 1)
    return;

  thread_pool_stop_started(pool);

  thread_pool_clear(pool);

  if (sigprocmask(SIG_UNBLOCK, &block_mask, 0) == -1)
    exit(err);

}

int defer(struct thread_pool *pool, runnable_t runnable) {
  int err;

  if (sigprocmask(SIG_BLOCK, &block_mask, 0) == -1)
    exit(-1);

  if (sem_wait(&(pool->lock2)) != 0)
    return -1;

  if ((err = pthread_mutex_lock(&(pool->lock))) != 0) {
    thread_pool_clear(pool);
    exit(err);
  }

  if (pool->destroyed) {
    if ((err = pthread_mutex_unlock(&(pool->lock))) != 0) {
      thread_pool_clear(pool);
      exit(err);
    }

    return -1;
  }

  if (fifo_push(pool->fifo, &runnable) != 0) {
    thread_pool_clear(pool);
    exit(-1);
  }

  if ((err = pthread_cond_signal(&(pool->task_waiting))) != 0) {
    thread_pool_clear(pool);
    exit(err);
  }

  if ((err = pthread_mutex_unlock(&(pool->lock))) != 0) {
    thread_pool_clear(pool);
    exit(err);
  }

  if ((err = sem_post(&(pool->lock2))) != 0)
    exit(err);

  if (sigprocmask(SIG_UNBLOCK, &block_mask, 0) == -1)
    exit(-1);

  return 0;
}