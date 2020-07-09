//
// Created by adam on 30.12.2019.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "future.h"

static thread_pool_t pool;


typedef struct {
  int row;
  int column;
  int max_columns; // numbers of columns in a matrix
  int sum; // sum of this element and predecessing in a row
  int* tab; // pointer to a tab with values of matrix cells
  int* times; // pointer to matrix of times to wait

} arg_matrix;

// to add next cells in a row
static void *add(void *arg, size_t argsz __attribute__((unused)),
                      size_t *retsz __attribute__((unused))) {
  arg_matrix* n = (arg_matrix *)arg; // is to be freed by user, as it is a value
  arg_matrix *ret = malloc(sizeof(arg_matrix));
  if (ret == NULL) {
    free(n);
    exit(-1);
  }


  *retsz = sizeof(arg_matrix);

  int err;
  // sleeping
  if ((err = usleep(n->times[n->row * n->max_columns + n->column])) != 0)
    exit(err);

  // calculating passing arguments
  ret->sum = n->sum + n->tab[n->row * n->max_columns + n->column];
  ret->row = n->row;
  ret->column = n->column + 1;
  ret->max_columns = n->max_columns;
  ret->tab = n->tab;
  ret->times = n->times;

  return ret;
}

// to add first cell cells in a row
static void *start_matrix(void *arg, size_t argsz __attribute__((unused)),
                          size_t *retsz __attribute__((unused))) {

  arg_matrix* n = (arg_matrix *)arg; // we have to free it
  arg_matrix* ret = malloc(sizeof(arg_matrix));
  if (ret == NULL) {
    free(n);
    exit(-1);
  }
  int err;

  const struct timespec x = (const struct timespec) {
    .tv_nsec = n->times[n->row * n->max_columns + n->column] * 1000000,
    .tv_sec = 0
  };
  if ((err = nanosleep(&x, NULL)) != 0)
    exit(err);

  ret->sum = n->tab[n->row * n->max_columns + n->column];
  ret->row = n->row;
  ret->column = 1;
  ret->max_columns = n->max_columns;
  ret->tab = n->tab;
  ret->times = n->times;

  free(n);
  return ret;
}

int main() {
  int err = 0, n, k;



  scanf("%d%d", &n, &k);

  future_t future[n][k];
  int* tab = (int*)malloc(sizeof(int) * n * k);
  int* times = (int*)malloc(sizeof(int) * n * k);

  if (tab == NULL || times == NULL || thread_pool_init(&pool, 4) != 0) {
    free(tab);
    free(times);
    return -1;
  }

  // calculating
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < k; j++) {
      scanf("%d%d", &tab[i * k + j], &times[i * k + j]);
      if (j == 0) {
        // will be freed in function
        arg_matrix* x =(arg_matrix*)malloc(sizeof(arg_matrix));

        x->column = 0;
        x->max_columns = k;
        x->row = i;
        x->sum = 0;
        x->tab = tab;
        x->times = times;

        async(&pool, &future[i][0], (callable_t){
          .function = start_matrix,
          .arg = x,
          .argsz = sizeof(arg_matrix)
        });
      }
      else {
        map(&pool, &future[i][j], &future[i][j - 1], add);
      }
    }
  }

  // reading results
  for (int i = 0; i < n; i++) {
    arg_matrix* result = (arg_matrix*)(await(&future[i][k - 1]));
    printf("%d\n", result->sum);


    for (int j = 0; j < k; j++) {
      free(future[i][j].value);
      if (pthread_cond_destroy(&(future[i][j].calculated)) != 0) {
        err = -1;
      }
      if (pthread_mutex_destroy(&(future[i][j].lock)) != 0) {
        err = -1;
      }
    }
  }

  thread_pool_destroy(&pool);
  return err;
}