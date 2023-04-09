// ? Loc here: header modification to adapt pthread_setaffinity_np
#include <assert.h>
#define __USE_GNU
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utmpx.h>
#define _GNU_SOURCE

void *thread1(void *dummy) {
  assert(sched_getcpu() == 0);
  return NULL;
}

void *thread2(void *dummy) {
  assert(sched_getcpu() == 1);
  return NULL;
}

cpu_set_t cpuset;
int main() {
  pthread_t pid[2];
  int i;
  // ? LoC: Bind core here
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset);

  // 1 Loc code here: create thread and save in pid[2]
  pthread_create(&pid[0], NULL, thread1, NULL);
  pthread_create(&pid[1], NULL, thread2, NULL);

  pthread_setaffinity_np(pid[0], sizeof(cpuset), &cpuset);
  CPU_CLR(0, &cpuset);
  CPU_SET(1, &cpuset);
  pthread_setaffinity_np(pid[1], sizeof(cpuset), &cpuset);

  for (i = 0; i < 2; ++i) {
    // 1 Loc code here: join thread
    pthread_join(pid[i], NULL);
  }
  return 0;
}
