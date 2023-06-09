# Thread and Coroutine

## Thread

### Task1

```c
#include <pthread.h>
// 创建进程
int pthread_create(
                pthread_t *restrict tidp, //新创建的线程ID指向的内存单元。
                const pthread_attr_t *restrict attr, //线程属性，默认为NULL
                void *(*start_routine)(void *), //新创建的线程从start_routine函数的地址开始运行
                void *restrict arg //默认为NULL。若start_routine函数需要参数，将参数放入结构中										 并将地址作为arg传入。
                  );
// 如果成功创建线程，pthread_create() 函数返回数字 0，反之返回非零值

// 获取某个线程执行结束时返回的数据 
int pthread_join(pthread_t thread, //指定接收的进程 
                 void ** retval //接收到的返回值，如果没有返回值或者不需要返回值，可以设为NULL
                );
```

## Coroutine

### X86 寄存器

* `%rdi` 是第一个传参寄存器
* `%rsi` 是第二个传参寄存器

* `%rsp` 是**堆栈指针寄存器**，通常会指向栈顶位置，堆栈的 pop 和push 操作就是通过改变 `%rsp` 的值即移动堆栈指针的位置来实现的
* `%rbp` 是栈帧指针，用于标识当前栈帧的起始位置

* `%rbx` 是通用寄存器，可以看作变量

### 实现思路

#### 单线程

* **coroutine_switch**

  参数 save 为 caller/callee Registers, 地址在传参数的时候存在 `%rdi` 中，之后把物理寄存器的值存到 `%rdi` 起始的一段模拟寄存器中的对应“寄存器”中，具体 asm 实现代码如下：

  ```shell
  # 第一次进入线程的时候 %rip 存的是 init 函数的地址
  # 复制提前设置的寄存器参数后跳转到 init 之后 (jmpq  *120(%rsi) )
  # switch 的时候把模拟寄存器 %rip 处的值改成 ret 函数的地址
  # leaq .coroutine_ret(%rip), %rbx + movq  %rbx, 120(%rdi)
  # 以后再继续执行线程的时候，复制之前的%rip参数后会跳转到 ret 函数
  .global coroutine_init
  coroutine_init:
      movq %r13, %rdi
      callq *%r12
  
  .global coroutine_switch
  coroutine_switch:
      # 保存 callee-saved 寄存器到 %rdi 的上下文
      # 64 为 %rsp 在模拟寄存器中的偏移量
      movq  %rsp, 64(%rdi)
      movq  %rbx, 72(%rdi)
      movq  %rbp, 80(%rdi)
      movq  %r12, 88(%rdi)
      movq  %r13, 96(%rdi)
      movq  %r14, 104(%rdi)
      movq  %r15, 112(%rdi)
  
      # 保存 ret 指令的地址（coroutine_ret）
      leaq .coroutine_ret(%rip), %rbx
      movq  %rbx, 120(%rdi)
  
      # 从 %rsi 指向的上下文恢复 callee-saved 寄存器
      movq  64(%rsi),  %rsp
      movq  72(%rsi), %rbx
      movq  80(%rsi), %rbp
      movq  88(%rsi),  %r12
      movq  96(%rsi),  %r13
      movq  104(%rsi),  %r14
      movq  112(%rsi),  %r15
  
      # 第一次进线程跳转到 init,之后跳转到 ret
      jmpq  *120(%rsi)    
  
  .coroutine_ret:
      ret
  ```

* **yield**

  * 没有协程可以切换，继续执行当前协程

  * yield 之后切换成新的协程
  
    由于切换的协程执行完之后需要把 `current_coroutine` 修改成 `yield()` 函数所在的原协程，因此需要在每个协程中保存调用的协程 `caller_coroutine`
#### 多线程

* 使用 `pthread_self()` 函数获得当前协程的 `pid`，使用 `pthread_equal()` 函数比较两个 `pid` 是否相等

* 现在给每一个线程开一个单独的协程池 `coroutine_pool`，把原来的一个协程池改成了一个协程池数组，每个协程池中存对应协程的 `pid`

* 在 `co_start()` 函数中给新的线程开协程池

  ```c
    int current_pool_id = search_pool_id(pthread_self());
    // initialize coroutine_pool
    if (current_pool_id == -1) {
      current_pool = create_coroutine_pool();
      my_pool[pool_cnt++] = current_pool;
      current_pool->pid = pthread_self();
    } else
      current_pool = my_pool[current_pool_id];
  ```

* 上锁：

  只有协程池相关的全局变量是不同线程共用的。另外，只有 `co_start()` 函数中会修改协程池，且某个线程对于协程池的修改不会影响其他的线程对于协程池的查询（修改是在最后加一个新的协程池），所以只需要给上面 `co_start()` 函数的代码加上锁即可（防止一个线程的多个协程同时调用 `co_start()` 函数从而给同一个线程创建多个协程池）。

### C 

* C 语言中类 (struct) 没有构造函数，析构函数和成员函数，解决方法：

  ```c
  coroutine_context *create_coroutine_context();
  void destruct_coroutine_context(coroutine_context *);
  ```

* struct 类在声明时需要加上 struct 关键字，也可以提前声明：

  ```c
  typedef struct coroutine_pool coroutine_pool;
  ```

