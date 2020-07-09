#include <stdlib.h>
#include <stdio.h>
#include "future.h"

//typedef void *(*function_t)(void *);

int init_future = 0;

typedef struct {
  future_t* future; /* future with place for a value */
  callable_t callable; /* structure with function and value */
} async_arg;


typedef struct {
  future_t* from; /* future with place with an argument */
  future_t* to; /* future with place for a value */
  void *(*function)(void *, size_t, size_t *);
} map_arg;

static void function_def_async(void *arg, size_t argsz __attribute__((unused))) {
  int err;

  // our structure with future and callable; it has to be freed
  async_arg* n = (async_arg*)arg;

  if ((err = pthread_mutex_lock(&(n->future->lock))) != 0) {
    exit(err);
  }

  void *(*function)(void *, size_t, size_t *) = n->callable.function;
  void* farg = n->callable.arg;
  size_t fargsz = n->callable.argsz;
  size_t* fvsz = &(n->future->value_size);

  // setting a pointer to a value allocated
  n->future->value = (*function)(farg, fargsz, fvsz);

  n->future->being_counted = 0;

  if ((err = pthread_mutex_unlock(&(n->future->lock))) != 0) {
    exit(err);
  }

  if ((err = pthread_cond_signal(&(n->future->calculated))) != 0) {
    exit(err);
  }

  free(n);
}

static void function_def_map(void *arg, size_t argsz __attribute__((unused))) {
  int err;


  map_arg* n = (map_arg*)arg; // our structure; it has to be freed
  void *result = await(n->from);  // our argument; it is to be freed by user

  if ((err = pthread_mutex_lock(&(n->to->lock))) != 0) {
    exit(err);
  }

  // setting a pointer to a value allocated
  n->to->value = (*(n->function))(result, n->from->value_size, &(n->to->value_size));
  n->to->being_counted = 0;

  if ((err = pthread_mutex_unlock(&(n->to->lock))) != 0) {
    exit(err);
  }

  if ((err = pthread_cond_signal(&(n->to->calculated))) != 0) {
    exit(err);
  }

  free(n);
}


int async(thread_pool_t *pool, future_t *future, callable_t callable) {
  int err;
  int result = 0;

  if (init_future == 0) {
    init_future = 1;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
  }

  if (sigprocmask(SIG_BLOCK, &block_mask, 0) == -1)
    exit(-1);

  if (pool->link == NULL)
    return -1;

  if (future == NULL)
    return -1;

  if ((sem_init(&(future->integral), 0, 0), NULL) != 0) {
    return -1;
  }

  if (pthread_mutex_init(&(future->lock), NULL) != 0) {
    if ((err = sem_destroy(&(future->integral))) != 0)
      exit(err);
    return -1;
  }


  if (pthread_mutex_lock(&(future->lock)) != 0) {
    if ((err = sem_destroy(&(future->integral))) != 0)
      exit(err);
    if ((err = pthread_mutex_destroy(&(future->lock))) != 0) {
      exit(err);
    }

    return -1;
  }

  if ((err = sem_post(&(future->integral))) != 0) {
    int err2;

    if ((err2 = pthread_mutex_unlock(&(future->lock))) != 0) {
      exit(err2);
    }

    if ((err2 = pthread_mutex_destroy(&(future->lock))) != 0) {
      exit(err2);
    }

    if ((err2 = sem_destroy(&(future->integral))) != 0)
      exit(err2);

    exit(err);
  }


  future->being_counted = 1;

  if (pthread_cond_init(&(future->calculated), NULL) != 0) {
    if ((err = sem_destroy(&(future->integral))) != 0)
      exit(err);

    if ((err = pthread_mutex_unlock(&(future->lock))) != 0) {
      exit(err);
    }

    if ((err = pthread_mutex_destroy(&(future->lock))) != 0) {
      exit(err);
    }

    return -1;
  }

  async_arg* n = (async_arg*)malloc(sizeof(async_arg));
  n->callable = callable;

  n->future = future;

  if (defer(pool, (runnable_t) {
    .function = function_def_async,
    .arg = n,
    .argsz = sizeof(async_arg)
  }) != 0) {
    free(n);
    future->being_counted = 2;
    result  = -1;
  } // placing a function setting value to a pool

  if (pthread_mutex_unlock(&(future->lock)) != 0) {
    if ((err = pthread_mutex_destroy(&(future->lock))) != 0) {
      exit(err);
    }
    if ((err = pthread_cond_destroy(&(future->calculated))) != 0) {
      exit(err);
    }
    if ((err = sem_destroy(&(future->integral))) != 0)
      exit(err);

    return -1;
  }

  return result;
}

int map(thread_pool_t *pool, future_t *future, future_t *from,
        void *(*function)(void *, size_t, size_t *)) {

  if (sigprocmask(SIG_BLOCK, &block_mask, 0) == -1)
    exit(-1);

  if (pool->link == NULL)
    return -1;

  int err;
  int result = 0;

  if (future == NULL)
    return -1;


  if ((sem_init(&(future->integral), 0, 0), NULL) != 0) {
    return -1;
  }


  if (pthread_mutex_init(&(future->lock), NULL) != 0) {
    if ((err = sem_destroy(&(future->integral))) != 0)
      exit(err);
    return -1;
  }


  if (pthread_mutex_lock(&(future->lock)) != 0) {
    if ((err = sem_destroy(&(future->integral))) != 0)
      exit(err);
    if ((err = pthread_mutex_destroy(&(future->lock))) != 0) {
      exit(err);
    }

    return -1;
  }

  if ((err = sem_post(&(future->integral))) != 0) {
    int err2;
    if ((err2 = sem_destroy(&(future->integral))) != 0)
      exit(err2);
    if ((err2 = pthread_mutex_destroy(&(future->lock))) != 0) {
      exit(err2);
    }
    exit(err);
  }


  future->being_counted = 1;

  if (pthread_cond_init(&(future->calculated), NULL) != 0) {
    if ((err = pthread_mutex_destroy(&(future->lock))) != 0) {
      exit(err);
    }
    if ((err = sem_destroy(&(future->integral))) != 0) {
      exit(err);
    }
    return -1;
  }
  future->value_size = 0;

  map_arg* n = (map_arg*)malloc(sizeof(map_arg));
  n->from = from;
  n->to = future;
  n->function = function;

  if (defer(pool, (runnable_t) {
    .function = function_def_map,
    .arg = n,
    .argsz = sizeof(map_arg)
  }) != 0) {
    free(n);
    future->being_counted = 2;
    result  = -1;
  } // placing a function setting value to a pool

  if (pthread_mutex_unlock(&(future->lock)) != 0) {
    if ((err = pthread_mutex_destroy(&(future->lock))) != 0) {
      exit(err);
    }
    if ((err = pthread_cond_destroy(&(future->calculated))) != 0) {
      exit(err);
    }

    if ((err = sem_destroy(&(future->integral))) != 0) {
      exit(err);
    }

    return -1;
  }

  return result;
}

void *await(future_t *future) {
  int err;

  if ((err = sem_wait(&(future->integral))) != 0) {
    exit(err);
  }

  if ((err = pthread_mutex_lock(&(future->lock))) != 0) {
    exit(err);
  }

  if ((err = sem_post(&(future->integral))) != 0) {
    exit(err);
  }

  // waiting for calculating to finish
  while (future->being_counted == 1) {

    if ((err = pthread_cond_wait(&(future->calculated), &(future->lock))) != 0) {
      exit(err);
    }

  }

  if ((err = pthread_mutex_unlock(&(future->lock))) != 0) {
    exit(err);
  }

  // calculation failure
  if (future->being_counted == 2) {
    exit(-1);
  }

  return future->value;
}
