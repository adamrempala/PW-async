#ifndef FUTURE_H
#define FUTURE_H

#include "threadpool.h"

typedef struct callable {
  void *(*function)(void *, size_t, size_t *);
  void *arg;
  size_t argsz;
} callable_t;

typedef struct future {
  sem_t integral; // protects stealing lock from async
  pthread_mutex_t lock; // mutex
  pthread_cond_t calculated; // signaled when value has been set
  size_t value_size; //size of value
  void* value; // pointer for a value
  int being_counted; // 1 if is being counted, 2 if failed, 0 otherwise
} future_t;

int async(thread_pool_t *pool, future_t *future, callable_t callable);

int map(thread_pool_t *pool, future_t *future, future_t *from,
        void *(*function)(void *, size_t, size_t *));

void *await(future_t *future);

#endif
