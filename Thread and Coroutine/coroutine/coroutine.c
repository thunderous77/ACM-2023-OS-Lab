#include "coroutine.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern void coroutine_init();
extern void coroutine_switch(unsigned long long *save,
                             unsigned long long *restore);
extern void coroutine_ret();

coroutine_context *create_coroutine_context(int (*routine)(void),
                                            coroutine_context *parent_coroutine,
                                            coroutine_pool *pool, cid_t cid) {
  coroutine_context *coroutine;
  coroutine = (coroutine_context *)malloc(sizeof(coroutine_context));
  coroutine->stack_size = 8 * 1024 / sizeof(unsigned long long);
  coroutine->stack = (unsigned long long *)malloc(coroutine->stack_size *
                                                  sizeof(unsigned long long));

  unsigned long long rsp =
      (unsigned long long)&coroutine->stack[coroutine->stack_size - 1];
  // 对齐到 16 字节
  rsp = rsp - (rsp & 0xF);

  void coroutine_main(coroutine_context * coroutine);

  coroutine->callee_registers[RSP] = rsp;
  // 协程入口是 coroutine_init
  coroutine->callee_registers[RIP] = (unsigned long long)coroutine_init;
  // 设置 r12 寄存器为 coroutine_main 的地址
  coroutine->callee_registers[R12] = (unsigned long long)coroutine_main;
  // 设置 r13 寄存器，用于 coroutine_main 的参数
  coroutine->callee_registers[R13] = (unsigned long long)coroutine;

  coroutine->func = routine;
  coroutine->retval = 0;
  coroutine->status = IDLE;
  coroutine->cid = cid;
  coroutine->caller_coroutine = NULL;

  coroutine->pool = pool;
  coroutine->parent_coroutine = parent_coroutine;
  coroutine->child_cnt = 0;

  return coroutine;
}

void destruct_coroutine_context(coroutine_context *coroutine) {
  for (int i = 0; i < coroutine->child_cnt; i++) {
    if (coroutine->child_coroutine[i] != NULL)
      destruct_coroutine_context(coroutine->child_coroutine[i]);
  }
  free(coroutine->stack);
  free(coroutine);
}

void coroutine_main(coroutine_context *coroutine) {
  coroutine->retval = (coroutine->func());
  coroutine->status = FINISHED;
  coroutine->pool->current_coroutine = coroutine->caller_coroutine;
  // 执行完后切换回调用 coroutine 的上下文
  coroutine_switch(coroutine->callee_registers, coroutine->caller_registers);
}

void resume(coroutine_context *old_coroutine,
            coroutine_context *new_coroutine) {
  // 防止 resume 已完成的协程
  if (new_coroutine->status != FINISHED)
    new_coroutine->status = RUNNING;
  else
    return;
  new_coroutine->pool->current_coroutine = new_coroutine;
  new_coroutine->caller_coroutine = old_coroutine;
  coroutine_switch(new_coroutine->caller_registers,
                   new_coroutine->callee_registers);
}

coroutine_pool *create_coroutine_pool() {
  coroutine_pool *pool;
  pool = (coroutine_pool *)malloc(sizeof(coroutine_pool));
  pool->coroutine_cnt = 0;
  pool->root_coroutine = create_coroutine_context(NULL, NULL, pool, -1);
  pool->current_coroutine = pool->root_coroutine;
  return pool;
}

void destruct_coroutine_pool(coroutine_pool **pool) {
  for (int i = 0; i < 50; i++) {
    coroutine_pool *current_pool = pool[i];
    if (pool != NULL) {
      destruct_coroutine_context(current_pool->root_coroutine);
      current_pool->coroutine_cnt = 0;
      free(current_pool);
    }
  }
}

// 加入子协程, 返回子协程
coroutine_context *add_coroutine(coroutine_pool *pool, int (*routine)(void)) {
  coroutine_context *parent_coroutine = pool->current_coroutine;
  coroutine_context *new_coroutine = create_coroutine_context(
      routine, parent_coroutine, pool, pool->coroutine_cnt);
  parent_coroutine->child_coroutine[parent_coroutine->child_cnt++] =
      new_coroutine;
  pool->coroutine_cnt++;
  return new_coroutine;
}

int pool_cnt = 0;
coroutine_pool *my_pool[MAXN] = {NULL};
pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;

coroutine_context *search_by_cid(cid_t cid, coroutine_context *coroutine) {
  if (coroutine->cid == cid) {
    return coroutine;
  } else {
    for (int i = 0; i < coroutine->child_cnt; i++) {
      coroutine_context *ret_coroutine =
          search_by_cid(cid, coroutine->child_coroutine[i]);
      if (ret_coroutine != NULL)
        return ret_coroutine;
    }
  }
  return NULL;
}

int search_pool_id(pthread_t current_pthread) {
  int ret_pool_id = -1;
  for (int i = 0; i < pool_cnt; i++) {
    if (my_pool[i] != NULL) {
      if (pthread_equal(my_pool[i]->pid, current_pthread)) {
        ret_pool_id = i;
        break;
      }
    }
  }
  return ret_pool_id;
}

int co_start(int (*routine)(void)) {
  coroutine_pool *current_pool;
  pthread_mutex_lock(&lk);
  int current_pool_id = search_pool_id(pthread_self());
  // initialize coroutine_pool
  if (current_pool_id == -1) {
    current_pool = create_coroutine_pool();
    my_pool[pool_cnt++] = current_pool;
    current_pool->pid = pthread_self();
  } else
    current_pool = my_pool[current_pool_id];
  pthread_mutex_unlock(&lk);
  coroutine_context *new_couroutine = add_coroutine(current_pool, routine);
  resume(current_pool->current_coroutine, new_couroutine);
  return new_couroutine->cid;
}

int co_getid() {
  coroutine_pool *current_pool;
  current_pool = my_pool[search_pool_id(pthread_self())];
  return current_pool->current_coroutine->cid;
}

int co_getret(int cid) {
  coroutine_pool *current_pool;
  current_pool = my_pool[search_pool_id(pthread_self())];
  return search_by_cid(cid, current_pool->root_coroutine)->retval;
}

int co_yield () {
  coroutine_pool *current_pool;
  current_pool = my_pool[search_pool_id(pthread_self())];
  cid_t yield_cid = current_pool->current_coroutine->cid;
  coroutine_context *resume_coroutine = NULL;
  for (cid_t cid = current_pool->coroutine_cnt - 1; cid >= 0; cid--) {
    resume_coroutine = search_by_cid(cid, current_pool->root_coroutine);
    if (resume_coroutine->status != FINISHED &&
        resume_coroutine->cid != yield_cid) {
      break;
    } else
      resume_coroutine = NULL;
  }
  // 如果没有其他协程切换就继续执行当前协程
  if (resume_coroutine != NULL) {
    resume(current_pool->current_coroutine, resume_coroutine);
  }
  return 0;
}

int co_waitall() {
  coroutine_pool *current_pool;
  current_pool = my_pool[search_pool_id(pthread_self())];
  for (cid_t cid = current_pool->coroutine_cnt - 1; cid >= 0; cid--)
    co_wait(cid);
  return 0;
}

int co_wait(int cid) {
  coroutine_pool *current_pool;
  current_pool = my_pool[search_pool_id(pthread_self())];
  coroutine_context *coroutine =
      search_by_cid(cid, current_pool->root_coroutine);
  while (coroutine->status != FINISHED) {
    resume(current_pool->root_coroutine, coroutine);
  }
  return 0;
}

int co_status(int cid) {
  coroutine_pool *current_pool;
  current_pool = my_pool[search_pool_id(pthread_self())];
  coroutine_context *coroutine =
      search_by_cid(cid, current_pool->root_coroutine);
  if (coroutine != NULL)
    return coroutine->status;
  else
    return UNAUTHORIZED;
}