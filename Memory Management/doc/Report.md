# Buddy and Malloc

## Buddy 内存分配

### 实现细节

* 每个大小的 buddy_page 都是一个单向链表（Implicit free list），内存地址从小到大排列
  
* 对于 buddy_page 的 merge
  * return_pages 时，将 return 的 buddy_page 与前后相匹配（由同一个上一级 buddy_page split 而来）的 buddy_page 进行 merge，只 merge 一次
  
  * create_buddy_page 时，将新创建的 buddy_page 与前后相匹配的 buddy_page 进行 merge，递归向上 merge

## Malloc 动态存储分配器

### 读题思考 + 对应实现

* 题目给出了一个信息--堆的大小将永远不会大于或等于 $2^{32}$ 字节，那么我们就可以把本来用于定位的指针（8 byte）优化成一个 4 byte = 32 bit 的 offset

* 三种数据结构：

  * `Implicit free list`

    把所有块连接起来，每次分配时从头到尾扫描合适的空闲块
    
    <img src="https://pic2.zhimg.com/80/v2-092fc522316822ae3c5508a321c98715_720w.webp" alt="img" style="zoom:50%;" />
    
    这里 31 - 0 就是之前堆大小限制的优化
    
    由于 8 字节对齐，最后 3 位一定是 0，可以用来储存是否被 allocated 的信息
    
    ```c
    #define PACK(size, alloc) ((size) | (alloc))
    ```
    
    Payload 储存 allocated 的块中的内容，而 Padding 用来对齐
    
    前后都存 Block size 可以以常数时间查询块大小，从而以常数时间与前或后的块合并
    
    ![img](https://pic2.zhimg.com/80/v2-b9cfe142c46c0f10604d29ed36beea19_720w.webp)
    
    Prologue block 和 Epilogue block 都是为了方便合并空闲块，而第一个则是为了对齐 8 byte
    
  * `Explicit free list`
  
    每个空闲块都记录了前一个空闲块和后一个空闲块
  
    复杂度：O(块总数) $\to$ O(空闲块总数)
  
  * `Segregated free list`
  
* 宏尽量加上括号

  定义宏 `#define twice(x) 2*x`，调用 `twice(x+1)` 会变成 `2x+1`，因为宏是简单调用

### 实现细节



### 测试结果

* ```shell
  # mm-naive.c
  # Lab 中给出的基础实现，即从不 free 任何空间
  Processor clock rate ~= 2304.0 MHz
  ....................
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    23%    4805  0.000017286050 ./traces/amptjp.rep
   * yes    19%    5032  0.000014351282 ./traces/cccp.rep
   * yes     0%   14400  0.000034422537 ./traces/coalescing-bal.rep
     yes   100%      15  0.000000278710 ./traces/corners.rep
   * yes    30%    5683  0.000016364420 ./traces/cp-decl.rep
   * yes    68%     118  0.000000409446 ./traces/hostname.rep
   * yes    65%   19405  0.000046422350 ./traces/login.rep
   * yes    75%     372  0.000001452050 ./traces/ls.rep
     yes    77%      17  0.000000349714 ./traces/malloc-free.rep
     yes    94%      10  0.000000267907 ./traces/malloc.rep
   * yes    71%    1494  0.000004424855 ./traces/perl.rep
   * yes    36%    4800  0.000016297626 ./traces/random.rep
   * yes    83%     147  0.000000435331 ./traces/rm.rep
     yes   100%      12  0.000000329143 ./traces/short2.rep
   * yes    44%   57716  0.000120481318 ./traces/boat.rep
   * yes    25%     200  0.000000482008 ./traces/lrucd.rep
   * yes     0%  100000  0.000530188694 ./traces/alaska.rep
   * yes    34%     200  0.000000445648 ./traces/nlydf.rep
   * yes    32%     200  0.000000441379 ./traces/qyqyc.rep
   * yes    28%     200  0.000000424309 ./traces/rulsr.rep
  16        40%  214772  0.000799268635
  
  Perf index = 0 (util) + 37 (thru) = 37/100
  ```

* 
