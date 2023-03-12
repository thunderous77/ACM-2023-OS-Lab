#include "coroutine.h"
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
  // 执行完后切换回调用 coroutine 的上下文
  coroutine_switch(coroutine->callee_registers, coroutine->caller_registers);
}

// 没有协程可以切换,直接切换回调用 yield 的上下文
void yield(coroutine_context *coroutine) {
  // 当前协程变为 root 线程
  coroutine->pool->current_coroutine = coroutine->pool->root_coroutine;
  coroutine->status = IDLE;
  coroutine_switch(coroutine->callee_registers, coroutine->caller_registers);
}

void resume(coroutine_context *coroutine) {
  coroutine->pool->current_coroutine = coroutine;
  // 防止 resume 已完成的协程
  if (coroutine->status != FINISHED)
    coroutine->status = RUNNING;
  else
    return;
  coroutine_switch(coroutine->caller_registers, coroutine->callee_registers);
}

/* 这里 yield + resume 会出现问题, yield 执行 switch 之后直接切回调用 yield
的上下文了, 需要保存 old callee registers,向物理寄存器内存入 new callee
registers 并转移 caller registers */
void yield_resume(coroutine_context *old_coroutine,
                  coroutine_context *new_coroutine) {
  new_coroutine->pool->current_coroutine = new_coroutine;
  old_coroutine->status = IDLE;
  // 之前 search 保证新协程没有完成
  new_coroutine->status = RUNNING;
  for (int i = 0; i < RegisterCount; ++i)
    new_coroutine->caller_registers[i] = old_coroutine->caller_registers[i];
  coroutine_switch(old_coroutine->callee_registers,
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

void destruct_coroutine_pool(coroutine_pool *pool) {
  destruct_coroutine_context(pool->root_coroutine);
  pool->coroutine_cnt = 0;
  free(pool);
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

coroutine_pool *my_pool = NULL;

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

int co_start(int (*routine)(void)) {
  // initialize coroutine_pool
  if (my_pool == NULL)
    my_pool = create_coroutine_pool();
  coroutine_context *new_couroutine = add_coroutine(my_pool, routine);
  resume(new_couroutine);
  return new_couroutine->cid;
}

int co_getid() { return my_pool->current_coroutine->cid; }

int co_getret(int cid) {
  return search_by_cid(cid, my_pool->root_coroutine)->retval;
}

int co_yield () {
  cid_t yield_cid = my_pool->current_coroutine->cid;
  coroutine_context *resume_coroutine = NULL;
  for (cid_t cid = my_pool->coroutine_cnt - 1; cid >= 0; cid--) {
    resume_coroutine = search_by_cid(cid, my_pool->root_coroutine);
    if (resume_coroutine->status != FINISHED &&
        resume_coroutine->cid != yield_cid) {
      break;
    } else
      resume_coroutine = NULL;
  }
  // 有可能之前的协程被 yield 之后没有新的协程 resume
  if (resume_coroutine == NULL) {
    // 直接切换成调用 yield 的上下文
    yield(my_pool->current_coroutine);
  } else {
    yield_resume(my_pool->current_coroutine, resume_coroutine);
  }
  return 0;
}

int co_waitall() {
  for (cid_t cid = my_pool->coroutine_cnt - 1; cid >= 0; cid--)
    co_wait(cid);
  return 0;
}

int co_wait(int cid) {
  coroutine_context *coroutine = search_by_cid(cid, my_pool->root_coroutine);
  while (coroutine->status != FINISHED) {
    resume(coroutine);
  }
  return 0;
}

int co_status(int cid) {
  coroutine_context *coroutine = search_by_cid(cid, my_pool->root_coroutine);
  if (coroutine != NULL)
    return coroutine->status;
  else
    return UNAUTHORIZED;
}