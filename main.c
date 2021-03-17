#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct task_add_args_s {
  int a;
  int b;
} task_add_args_t;

struct task_s;
typedef struct task_s task_t;
struct task_s {
  int id;
  bool done;
  int dependencies_len;
  task_t **dependencies;
  int dependents_len;
  task_t **dependents;
  pthread_mutex_t *mutex;
  void *(*fn)(void *);
  void *args;
  void *result;
};

void *task_add(void *args) {
  task_t *task = args;
  pthread_mutex_lock(task->mutex);
  printf("id=%d, acquired lock\n", task->id);
  task_add_args_t *add_args = task->args;
  int a = 0, b = 0;
  pthread_mutex_t *mutex_a = NULL, *mutex_b = NULL;
  bool locked_mutex_a = false;
  bool locked_mutex_b = false;
  if (task->dependencies_len == 0) {
    printf("id=%d, no depends\n", task->id);
    a = add_args->a;
    b = add_args->b;
  } else if (task->dependencies_len == 1) {
    printf("id=%d, one depends\n", task->id);
    mutex_a = task->dependencies[0]->mutex;
    pthread_mutex_lock(mutex_a);
    locked_mutex_a = true;
    printf("id=%d, acquired lock dep1\n", task->id);
    a = *(int *)task->dependencies[0]->result;
    b = add_args->b;
  } else if (task->dependencies_len == 2) {
    printf("id=%d, two depends\n", task->id);
    mutex_a = task->dependencies[0]->mutex;
    mutex_b = task->dependencies[1]->mutex;
    pthread_mutex_lock(mutex_a);
    printf("id=%d, acquired lock dep1\n", task->id);
    locked_mutex_a = true;
    pthread_mutex_lock(mutex_b);
    printf("id=%d, acquired lock dep2\n", task->id);
    locked_mutex_b = true;
    a = *(int *)task->dependencies[0]->result;
    b = *(int *)task->dependencies[1]->result;
  }
  int *sum = task->result;
  *sum = a + b;
  if (locked_mutex_a == true) {
    pthread_mutex_unlock(mutex_a);
    printf("id=%d, released lock dep1\n", task->id);
  }
  if (locked_mutex_b == true) {
    pthread_mutex_unlock(mutex_b);
    printf("id=%d, released lock dep2\n", task->id);
  }
  task->done = true;
  pthread_mutex_unlock(task->mutex);
  printf("id=%d, released lock\n", task->id);
  return sum;
}

void task_add_dependency(task_t *task, task_t *dependency) {

  if (dependency->id != 0) {
    int copy_sz = (sizeof *task->dependencies) * task->dependencies_len;
    int alloc_sz = copy_sz + (sizeof *task->dependencies);
    task_t **dependencies = malloc(alloc_sz);
    memcpy(dependencies, task->dependencies, copy_sz);
    int idx = task->dependencies_len;
    dependencies[idx] = dependency;
    free(task->dependencies);
    task->dependencies = dependencies;
    task->dependencies_len += 1;
  }

  int copy_sz2 = (sizeof *task->dependents) * dependency->dependents_len;
  int alloc_sz2 = copy_sz2 + (sizeof *task->dependents);
  task_t **dependents = malloc(alloc_sz2);
  memcpy(dependents, dependency->dependents, copy_sz2);
  int idx2 = dependency->dependents_len;
  dependents[idx2] = task;
  free(dependency->dependents);
  dependency->dependents = dependents;
  dependency->dependents_len += 1;

}

task_t *task_alloc(void *(*fn)(void *), int id) {
  task_t *task = malloc(sizeof *task);
  task->id = id;
  task->done = false;
  task->dependencies_len = 0;
  task->dependencies = malloc(sizeof *task->dependencies);
  memset(task->dependencies, 0, sizeof *task->dependencies);
  task->dependents_len = 0;
  task->dependents = malloc(sizeof *task->dependents);
  memset(task->dependents, 0, sizeof *task->dependents);
  task->mutex = malloc(sizeof *(task->mutex));
  pthread_mutex_init(task->mutex, NULL);
  task->fn = fn;
  if (fn == task_add) {
    task_add_args_t *args = malloc(sizeof *args);
    int *result = malloc(sizeof *result);
    memset(args, 0, sizeof *args);
    memset(result, 0, sizeof *result);
    task->args = args;
    task->result = result;
  } else if (fn == NULL) {
    task->args = NULL;
    task->result = NULL;
  }
  return task;
}

void task_dealloc(task_t *task, bool dealloc_dependencies) {
  free(task->args);
  free(task->result);
  pthread_mutex_destroy(task->mutex);
  if (dealloc_dependencies) {
    for (int i = 0, max = task->dependencies_len; i < max; i++) {
      task_dealloc(task->dependencies[i], dealloc_dependencies);
    }
  }
  free(task->dependents);
  free(task->dependencies);
  free(task);
}

void task_add_set_a(task_t *task, int a) {
  task_add_args_t *args = task->args;
  args->a = a;
}

void task_add_set_b(task_t *task, int b) {
  task_add_args_t *args = task->args;
  args->b = b;
}

void *find_task(task_t *task) {
  if (task->id == 0 && task->done == false) {
    for (int i = 0, max = task->dependents_len; i < max; i++) {
      task_t *ready_task = task->dependents[i];
      if (ready_task->done == false) {
        return ready_task;
      }
    }
    task->done = true;
  }
  if (task->done == false) {
    bool can_do = true;
    for (int i = 0, max = task->dependencies_len; i < max; i++) {
      task_t *dependency = task->dependencies[i];
      if (dependency->done == false) {
        can_do = false;
        break;
      }
    }
    if (can_do) {
      return task;
    }
  }
  else if (task->done == true) {
    for (int i = 0, max = task->dependents_len; i < max; i++) {
      task_t *dependent = task->dependents[i];
      task_t *maybe = find_task(dependent);
      if (maybe != NULL) {
        return maybe;
      }
    }
  }
  return NULL;
}

void do_tasks(void *args) {
  task_t *root = args;
  task_t *task = find_task(root);
  while (task != NULL) {
    task_t *task2 = find_task(root);
    pthread_t *pt = malloc(sizeof *pt);
    pthread_t *pt2 = malloc(sizeof *pt2);
    pthread_create(pt, NULL, task->fn, task);
    if (task2 != NULL) {
      pthread_create(pt2, NULL, task2->fn, task2);
    }
    pthread_join(*pt, NULL);
    pthread_join(*pt2, NULL);
    task = find_task(root);
    free(pt);
    free(pt2);
  }
}

int main () {
  task_t *root = task_alloc(NULL, 0);
  task_t *add1 = task_alloc(task_add, 1);
  task_t *add2 = task_alloc(task_add, 2);
  task_add_dependency(add1, root);
  task_add_set_a(add1, 1);
  task_add_set_b(add1, 2);
  task_add_dependency(add2, add1);
  task_add_set_b(add2, 3);
  task_t *add3 = task_alloc(task_add, 3);
  task_t *add4 = task_alloc(task_add, 4);
  task_add_dependency(add3, root);
  task_add_set_a(add3, 4);
  task_add_set_b(add3, 5);
  task_add_dependency(add4, add3);
  task_add_set_b(add4, 6);
  task_t *add5 = task_alloc(task_add, 5);
  task_add_dependency(add5, add2);
  task_add_dependency(add5, add4);
  
  do_tasks(root);
  
  printf("sum: %d\n", *(int *)add5->result);
  
  task_dealloc(root, true);

  return 0;
}
