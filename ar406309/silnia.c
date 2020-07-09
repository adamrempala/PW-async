//
// Created by adam on 30.12.2019.
//

#include <stdio.h>
#include <stdlib.h>
#include "future.h"

static thread_pool_t pool;

typedef struct {
  unsigned long long quotient;
  unsigned long long next_to_multiply;
} arg_silnia;

static void *multiply(void *arg, size_t argsz __attribute__((unused)),
                     size_t *retsz __attribute__((unused))) {
  arg_silnia* n = (arg_silnia *)arg;
  arg_silnia *ret = malloc(sizeof(arg_silnia));

  if (ret == NULL) {
    free(n);
    exit(-1);
  }

  *retsz = sizeof(arg_silnia);

  ret->quotient = n->quotient * n->next_to_multiply;
  ret->next_to_multiply = n->next_to_multiply + 1;

  return ret;
}

static void *start_silnia(void *arg __attribute__((unused)), size_t argsz __attribute__((unused)),
                          size_t *retsz __attribute__((unused))) {
  arg_silnia *ret = malloc(sizeof(arg_silnia));

  if (ret == NULL)
    exit(-1);

  ret->quotient = 1;
  ret->next_to_multiply = 2;

  return ret;
}

int main() {
  int err = 0;
  if (thread_pool_init(&pool, 3) != 0)
    return -1;

  unsigned long long n;
  scanf("%lld", &n);

  if (n > 0) {
    future_t future[n];
    if (async(&pool, &future[0], (callable_t){
      .function = start_silnia,
      .arg = &n,
      .argsz = sizeof(int)
    }) != 0) {
      thread_pool_destroy(&pool);
      return -1;
    }
    for (unsigned long long i = 2; i <= n; i++) {
      if (map(&pool, &future[i - 1], &future[i - 2], multiply) != 0) {
        for (unsigned long long j = 0; j < n - 1; j++) {
          free(future[j].value);
          pthread_cond_destroy(&(future[j].calculated));
          pthread_mutex_destroy(&(future[j].lock));
        }
        thread_pool_destroy(&pool);
        return -1;
      }
    }
    arg_silnia* result = (arg_silnia*)(await(&future[n - 1]));
    printf("%lld\n", result->quotient);


    for (unsigned long long i = 0; i < n; i++) {
      free(future[i].value);
      if (pthread_cond_destroy(&(future[i].calculated)) != 0) {
        err = -1;
      }
      if (pthread_mutex_destroy(&(future[i].lock)) != 0) {
        err = -1;
      }
    }

  }
  else {
    future_t future;
    if (async(&pool, &future, (callable_t){
      .function = start_silnia,
      .arg = &n,
      .argsz = sizeof(unsigned long long)
    }) != 0) {
      thread_pool_destroy(&pool);
      return -1;
    }

    arg_silnia* result = (arg_silnia*)(await(&future));

    printf("%lld\n", result->quotient);
    free(result);
    if (pthread_cond_destroy(&(future.calculated)) != 0) {
      err = -1;
    }
    if (pthread_mutex_destroy(&(future.lock)) != 0) {
      err = -1;
    }
  }

  thread_pool_destroy(&pool);
  return err;
}