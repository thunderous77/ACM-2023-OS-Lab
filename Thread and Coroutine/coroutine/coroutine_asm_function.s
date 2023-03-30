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
    