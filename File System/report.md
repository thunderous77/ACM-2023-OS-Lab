# File System

## mmap()

```
char* mmaped = (char *)mmap(void* start,size_t length,int prot,int flags,int fd,off_t offset);
```

作用：将一个文件或者其它对象映射进内存

参数：

* start：起始地址，一般为 NULL，起始地址由 mmap 函数返回
* length:映射大小
* prot：映射区域保护方式，由以下几种方式组合：
  * PROT_EXEC 映射区域可被执行
  * PROT_READ 映射区域可被读取
  * PROT_WRITE 映射区域可被写入
  * PROT_NONE 映射区域不能存取
* flag：映射区域特性，一般有：
  * MAP_SHARED--对映射区域的写入数据会复制回文件内，而且允许其他映射该文件的进程共享
  * MAP_PRIVATE--对映射区域的写入操作会产生一个映射文件的复制，即私人的“写入时复制”（copy on write）。对此区域作的任何修改都不会写回原来的文件内容
* fd：要映射到内存中的文件描述符
* offset：文件映射的偏移量，通常设置为0，从文件最前方开始计算，且必须是分页大小的整数倍

#### 用 mmap 修改文件

```c
mmaped = (char *)mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

// 修改 mmaped 内容
mmaped[0] = 'l';

// 将内存中的映射内容同步到内存
msync(mmaped, sb.st_size, MS_SYNC)；
```

#### 实现线程间通信

* 方法一：让两个线程将同一个文件分别映射到自己的内存空间，相当于：A写A的内存→同步到文件→文件映射到
* 方法二：使用匿名映射（MAP_ANONYMOUS）使得父子进程运行时访问同一段内存（先映射再 fork）

## fcntl()

```c
// 复制文件标识符
fcntl(fd, F_DUPFD);

// 设置写锁
struct flock fl;
fl.l_type = F_WRLCK;
// starting offset：SET-开头，CUR-当前位置，END-结尾
fl.l_whence = SEEK_SET;
// l_start 和 l_len 都设置为 0 就可以对整个文件上锁
fl.l_start = 0;
fl.l_len = 0;
fcntl(fd, F_SETLK, &fl);

// 获得锁的信息
fcntl(fd, F_GETLK, &fl);
```



## ioctl()



## FUSE

### libfuse

#### About

FUSE (Filesystem in Userspace) is an interface for userspace programs to export a filesystem to the Linux kernel. The FUSE project consists of two components: the *fuse* kernel module (maintained in the regular kernel repositories) and the *libfuse* userspace library (maintained in this repository). libfuse provides the reference implementation for communicating with the FUSE kernel module.

A FUSE file system is typically implemented as a standalone application that links with libfuse. libfuse provides functions to mount the file system, unmount it, read requests from the kernel, and send responses back. libfuse offers two APIs: a "high-level", synchronous API, and a "low-level" asynchronous API. In both cases, incoming requests from the kernel are passed to the main program using callbacks. When using the high-level API, the callbacks may work with file names and paths instead of inodes, and processing of a request finishes when the callback function returns. When using the low-level API, the callbacks must work with inodes and responses must be sent explicitly using a separate set of API functions.

#### Install

https://www.jianshu.com/p/040bb60aa468

### my_Fuse

#### 大致思路

先看了一下 libfuse 库里给的样例代码 `hello.c` 和 `fuse.h` 里可以实现的一些接口函数。

网上看到一个不错的[教程](https://blog.csdn.net/stayneckwind2/article/details/82876330)

* Diretory：实现了 `getattr(),mkdir(),rmdir()`，就可以执行 `cd,mkdir,rmdir` 指令（需要实现 `gerattr()` 函数是因为新建文件夹之前会先询问该路径的属性，查询读写权限）
* File：然后实现 `mknod(),open(),release(),unlink(),read(), write()`，就可以执行 `echo,cat,rm` 指令
* `ls`：实现了 `readdir()` 函数
* `touch`：实现了 `utimes()` 函数

#### [基于内核红黑树的数据结构](https://blog.csdn.net/stayneckwind2/article/details/82867062)

后来改成了基于单链表的实现，因为不让直接使用外部库

#### 实现细节

* ```c
  struct my_fuse_file *data = container_of(node, struct my_fuse_file, node);
  ```

  其中 `container_of ` 为 `rbtree.h` 中定义的宏，用于：get a pointer to the structure that contains a given field of that structure

  第一个参数是这个 field 的指针，第二个参数是这个数据结构的类型，第三个参数是这个 field 在数据结构中的名称

* ```c
  // example code of getattr() in "hello.c"
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
  } else if (strcmp(path+1, options.filename) == 0) {
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = strlen(options.contents);
  } else
      res = -ENOENT;
  ```
  
  `st_mode` 为指定文件系统对象（如文件或目录）的权限和类型

  `S_IFDIR` 和 `S_IFREG` 是在 `sys/stat.h` 头文件中定义的常量，分别表示目录和常规文件的文件类型

  值0755是一个八进制数字，表示目录的权限。第一个数字（0）表示数字以八进制格式表示。接下来的三个数字（755）分别表示所有者、组和其他人的权限。值7表示所有者的读、写和执行权限。值5表示组和其他人的读和执行权限。

  值0444是一个八进制数字，表示常规文件的权限。第一个数字（0）表示数字以八进制格式表示。接下来的三个数字（444）分别表示所有者、组和其他人的权限。值4表示所有用户的只读权限

  `st_mode` 为强链接

#### Bonus

实现了权限检查，同时实现了 chmod 指令
