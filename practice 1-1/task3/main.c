// ? Loc here: header modification to adapt pthread_barrier
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
// 2 Locs here: declare mutex and barrier
pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t bar;

void *thread1(void *dummy) {
  int i;
  pthread_barrier_wait(&bar);
  // 1 Loc: mutex operation
  pthread_mutex_lock(&lk);
  // 1 Loc: barrier operation
  printf("This is thread 1!\n");
  for (i = 0; i < 20; ++i) {
    printf("H");
    printf("e");
    printf("l");
    printf("l");
    printf("o");
    printf("W");
    printf("o");
    printf("r");
    printf("l");
    printf("d");
    printf("!");
  }
  // 1 Loc: mutex operation
  pthread_mutex_unlock(&lk);
  return NULL;
}

void *thread2(void *dummy) {
  int i;
  pthread_barrier_wait(&bar);
  // 1 Loc: mutex operation
  pthread_mutex_lock(&lk);
  // 1 Loc: barrier operation
  printf("This is thread 2!\n");
  for (i = 0; i < 20; ++i) {
    printf("A");
    printf("p");
    printf("p");
    printf("l");
    printf("e");
    printf("?");
  }
  // 1 Loc: mutex operation
  pthread_mutex_unlock(&lk);
  return NULL;
}
int main() {
  pthread_t pid[2];
  int i;
  // 1 Loc: barrier initialization
  pthread_barrier_init(&bar, NULL, 2);
  // 3 Locs here: create 2 thread using thread1 and thread2 as function.
  pthread_create(&pid[0], NULL, thread1, NULL);
  pthread_create(&pid[1], NULL, thread2, NULL);
  // mutex initialization
  pthread_mutex_init(&lk, NULL);
  // 1 Loc: barrier operation
  for (i = 0; i < 2; ++i) {
    // 1 Loc code here: join thread
    pthread_join(pid[i], NULL);
  }
  pthread_barrier_destroy(&bar);
  return 0;
}
