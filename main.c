#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *dummy(void *arg) {
  (void)arg;
  printf("spawned a thread\n");
  return NULL;
}

int main () {
  pthread_t* pt = malloc(sizeof *pt);
  pthread_create(pt, NULL, dummy, NULL);
  pthread_join(*pt, NULL);
  printf("joined a thread\n");
  return 0;
}
