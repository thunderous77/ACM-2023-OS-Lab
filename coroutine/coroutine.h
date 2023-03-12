// Reference : http://jyywiki.cn/OS/2022/labs/M2
#ifndef COROUTINE_H
#define COROUTINE_H

typedef long long cid_t;
#define MAXN 10
#define UNAUTHORIZED -1
#define FINISHED 2
#define RUNNING 1
#define IDLE 0

// 模拟寄存器
enum Registers {
  RAX = 0,
  RDI,
  RSI,
  RDX,
  R8,
  R9,
  R10,
  R11,
  RSP,
  RBX,
  RBP,
  R12,
  R13,
  R14,
  R15,
  RIP,
  // RET,
  RegisterCount
};

// coroutine 切换的汇编函数
void coroutine_init();
void coroutine_switch(unsigned long long *save, unsigned long long *restore);
void coroutine_ret();

typedef struct coroutine_pool coroutine_pool;
typedef struct coroutine_context coroutine_context;

// 栈大小默认为 16 KB
struct coroutine_context {
  unsigned long long *stack;
  unsigned long long stack_size;
  // 协程自己的寄存器
  unsigned long long callee_registers[RegisterCount];
  // 上一级协程/线程的寄存器
  unsigned long long caller_registers[RegisterCount];

  // 协程信息
  int (*func)();
  int retval;
  int status;
  cid_t cid;

  // 保存协程之间的关系
  coroutine_pool *pool;
  coroutine_context *parent_coroutine;
  coroutine_context *child_coroutine[MAXN];
  int child_cnt;
};

coroutine_context *create_coroutine_context(int (*routine)(void),
                                            coroutine_context *,
                                            coroutine_pool *, cid_t cid);

void destruct_coroutine_context(coroutine_context *);

void coroutine_main(coroutine_context *);

void yield(coroutine_context *);

void resume(coroutine_context *);

void yield_resume(coroutine_context *, coroutine_context *);

struct coroutine_pool {
  coroutine_context *current_coroutine;
  coroutine_context *root_coroutine;
  int coroutine_cnt;
};

coroutine_pool *create_coroutine_pool();

void destruct_coroutine_pool();

coroutine_context *add_coroutine(coroutine_pool *pool, int (*routine)(void));

// target function
// 返回cid_t
int co_start(int (*routine)(void));
int co_getid();
int co_getret(int cid);
int co_yield ();
int co_waitall();
int co_wait(int cid);
int co_status(int cid);

#endif
