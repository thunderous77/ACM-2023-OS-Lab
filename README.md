# ACM 2023 OS Lab
## Thread-and-Coroutine

### Schedule

- [x] Thread
- [x] Coroutine

### Introduction

* 学习使用 `pthread.h` 
* 用 `C` 语言实现简单的协程
* 实现多线程下的协程并维护线程安全
* 使用 `make && make run` 即可运行程序

## Memory Management

### Schedule

- [x] Buddy 内存分配
- [x] Malloc 动态存储分配器

### Introduction

* 了解 `Buddy` 内存分配算法
* 直接在物理内存上实现 `Implicit free list`，`Explicit free list`，`Segragated free list` 等数据结构
* 学习指针 debug 以及 malloc 优化

## File System

### Schedule

- [x] task 5
- [ ] task 6
- [x] FUSE

### Introduction

* 学习 `mmap(),fcntl(),ioctl()` 等函数的实现
* 了解体验 Fuse（Filesystem in Userspace），并基于 libfuse 实现简单的文件系统

## Networking and Security

### Schedule(only need to do one of the following)

- [ ] Socket Programming
- [x] CTF
- [ ] Reproduce a virus

### Introduction

* 学习使用 IDA，pwn 等工具实现逆向工程
* 学习 overflow，ROP 等技术
* 学习使用 GDB 获得程序运行的各种信息