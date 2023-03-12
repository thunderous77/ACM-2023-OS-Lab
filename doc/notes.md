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

  * 没有协程可以切换，直接切换回调用 yield 的上下文

    ```c
    void yield(coroutine_context *coroutine) {
      // 当前协程变为 root 线程
      coroutine->pool->current_coroutine = coroutine->pool->root_coroutine;
      coroutine->status = IDLE;
      coroutine_switch(coroutine->callee_registers, coroutine->caller_registers);
    }
    ```

  * yield 之后切换成新的协程

    这里 yield + resume 会出现问题， yield 执行 switch 之后直接切回调用 yield 的上下文了

    需要保存 old callee registers，向物理寄存器内存入 new callee registers 并转移 caller registers

    ```c
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
    ```

    

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


## Bonus: 实现协程内核隔离

[Reference](https://blog.csdn.net/whatday/article/details/104430169)

* Namespace

  **Namespace 是 Linux 内核用来隔离内核资源的方式。**通过 namespace 可以让一些进程只能看到与自己相关的一部分资源，而另外一些进程也只能看到与它们自己相关的资源，这两拨进程根本就感觉不到对方的存在。具体的实现方式是把一个或多个进程的相关资源指定在同一个 namespace 中。

* clone() 函数

  通过 clone() 在创建新进程的同时创建 namespace。

  ```c
  #define _GNU_SOURCE
  #include <sched.h>
  int clone(int (*fn)(void *), void *child_stack, int flags, void *arg);
  // fn：指定一个由新进程执行的函数。当这个函数返回时，子进程终止。该函数返回一个整数，表示子进程的退出代码。
  // child_stack：传入子进程使用的栈空间，也就是把用户态堆栈指针赋给子进程的 esp 寄存器(高位内存地址)。调用进程(指调用 clone() 的进程)应该总是为子进程分配新的堆栈。
  // flags：表示使用哪些 CLONE_ 开头的标志位，与 namespace 相关的有CLONE_NEWIPC、CLONE_NEWNET、CLONE_NEWNS、CLONE_NEWPID、CLONE_NEWUSER、CLONE_NEWUTS 和 CLONE_NEWCGROUP。
  // arg：指向传递给 fn() 函数的参数。
  ```

  

