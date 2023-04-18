# Buddy and Malloc

## Buddy 内存分配

### 实现细节

* 每个大小的 buddy_page 都是一个单向链表（Implicit free list），内存地址从小到大排列
* 对于 buddy_page 的 merge
  * return_pages 时，将 return 的 buddy_page 与前后相匹配（由同一个上一级 buddy_page split 而来）的 buddy_page 进行 merge，只 merge 一次
  * create_buddy_page 时，将新创建的 buddy_page 与前后相匹配的 buddy_page 进行 merge，递归向上 merge

### 新的思考

* 先分析一下现在的复杂度，alloc_pages 和 return_pages 都是 O(n) 的，因为都是遍历对应大小页的链表

* 如果再维护为每个大小的页再维护一个 free list，可以让 alloc_pages 变成 O(1)，但是必须同时维护一个已分配页的链表，因为 query_ranks 函数需要

* 突然得知了一个新的实现--bit map，大致记录一下

  * 假设总空间大小为 M，页大小为 m，那么共有 M/m 位，分别记录这一段地址是不是该大小的页

    例如一个 16K 的空间可以表示为：4K 页--1100，8K页--01，16K页--0

  * 只用 bit map 实现的话，alloc_pages 是 O(n) 的，因为要遍历整个 bit map，而 return_pages 是 O(1) 的，因为可以直接定位一个页的前后页，与之前相比相当于两者的时间复杂度反了一反

  * 不过现实中，我们一般要求 alloc 尽量快地完成，而 return 则是系统内部自己消化，所以原来的链表实现更合理

  * 所以 bit map 就没用了吗？其实 bit map 可以用来加快 query_ranks，因为 bit map 可以 O(1) 地访问指定地址的信息，我们只要在链表的基础上同时维护一个 bit map 即可

* 由于本人太懒了，第二个 lab 太难了，以上都是纸上谈兵，并没有实践过

## Malloc 动态存储分配器

### 读题思考

* 题目给出了一个信息--堆的大小将永远不会大于或等于 $2^{32}$ 字节，那么我们就可以把本来用于定位的指针（8 byte）优化成一个 4 byte = 32 bit 的 offset

  ```c
  /* get previous free block ptr */
  (int *)((long)(READ((char *)(ptr)-DSIZE)) + (long)(heap_list)))
   
  /* set previous free block ptr */
  (WRITE(((char *)(ptr)-DSIZE), (val - (long)(heap_list))))) 
  ```

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
  
    遍历复杂度：O(块总数) $\to$ O(空闲块总数)
  
    <img src="https://pic1.zhimg.com/80/v2-c0cb0926ad2e469833bfff9e3fffd2e4_720w.webp" alt="img" style="zoom:50%;" />
  
  * `Segregated free list`
  
* c 代码语法

  * 宏尽量加上括号
  
    定义宏 `#define twice(x) 2*x`，调用 `twice(x+1)` 会变成 `2x+1`，因为宏是简单调用
  
* 空间优化

  * 对于一个堆，可以从一边开始分配小的内存，一边开始分配大的内存

    具体情境比如连续分配了 $2^3,2^{10},2^3,2^{10}$ KB 的内存，free 了两个 $2^{10}$ KB 的内存，但是由于中间的 $2^3$ KB 的内存一直没有 free，导致请求更大的内存，比如 $2^{16}$ KB 的内存时在堆中找不到足够的空间，只能重新申请内存

### 实现过程

* 先测试一下 lab 里给出的样例代码

  ```shell
  # mm-naive.c
  # Lab 中给出的基础实现，即从不 free 任何空间
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

  空间利用率为 0，哈哈！

* 接下来按照 lab 中的建议，先实现一下 implicit free list

  ```shell
  # mm-IFL.c 
  # implement with Implicit free list and all three fit strategy
  
  # first fit
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.005063   949 ./traces/amptjp.rep
   * yes    99%    5032  0.004714  1067 ./traces/cccp.rep
   * yes    66%   14400  0.000077185925 ./traces/coalescing-bal.rep
     yes    96%      15  0.000000108000 ./traces/corners.rep
   * yes    99%    5683  0.008222   691 ./traces/cp-decl.rep
   * yes    75%     118  0.000010 11982 ./traces/hostname.rep
   * yes    90%   19405  0.145117   134 ./traces/login.rep
   * yes    88%     372  0.000049  7524 ./traces/ls.rep
     yes    28%      17  0.000000188308 ./traces/malloc-free.rep
     yes    34%      10  0.000000116364 ./traces/malloc.rep
   * yes    86%    1494  0.000937  1595 ./traces/perl.rep
   * yes    92%    4800  0.004295  1118 ./traces/random.rep
   * yes    79%     147  0.000013 11165 ./traces/rm.rep
     yes    89%      12  0.000000 94685 ./traces/short2.rep
   * yes    56%   57716  1.252728    46 ./traces/boat.rep
   * yes    63%     200  0.000002 82315 ./traces/lrucd.rep
   * yes    86%  100000  0.004251 23525 ./traces/alaska.rep
   * yes    89%     200  0.000004 53807 ./traces/nlydf.rep
   * yes    57%     200  0.000003 76190 ./traces/qyqyc.rep
   * yes    68%     200  0.000002 86747 ./traces/rulsr.rep
  16        81%  214772  1.425488   151
  
  Perf index = 40 (util) + 0 (thru) = 40/100
  
  # next fit
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    91%    4805  0.001136  4229 ./traces/amptjp.rep
   * yes    92%    5032  0.000715  7039 ./traces/cccp.rep
   * yes    66%   14400  0.000069207989 ./traces/coalescing-bal.rep
     yes    96%      15  0.000000146441 ./traces/corners.rep
   * yes    95%    5683  0.002347  2421 ./traces/cp-decl.rep
   * yes    75%     118  0.000001172071 ./traces/hostname.rep
   * yes    90%   19405  0.003209  6047 ./traces/login.rep
   * yes    88%     372  0.000003119073 ./traces/ls.rep
     yes    28%      17  0.000000192000 ./traces/malloc-free.rep
     yes    34%      10  0.000000149610 ./traces/malloc.rep
   * yes    81%    1494  0.000030 49350 ./traces/perl.rep
   * yes    91%    4800  0.002398  2002 ./traces/random.rep
   * yes    79%     147  0.000002 95084 ./traces/rm.rep
     yes    89%      12  0.000000119172 ./traces/short2.rep
   * yes    56%   57716  0.007906  7301 ./traces/boat.rep
   * yes    63%     200  0.000001209074 ./traces/lrucd.rep
   * yes    77%  100000  0.001254 79743 ./traces/alaska.rep
   * yes    76%     200  0.000001138212 ./traces/nlydf.rep
   * yes    57%     200  0.000001179300 ./traces/qyqyc.rep
   * yes    68%     200  0.000002126524 ./traces/rulsr.rep
  16        78%  214772  0.019075 11260
  
  Perf index = 34 (util) + 17 (thru) = 51/100
  
  # best fit
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.005797   829 ./traces/amptjp.rep
   * yes    99%    5032  0.005569   904 ./traces/cccp.rep
   * yes    66%   14400  0.000074195881 ./traces/coalescing-bal.rep
     yes    96%      15  0.000000 92406 ./traces/corners.rep
   * yes    99%    5683  0.009092   625 ./traces/cp-decl.rep
   * yes    75%     118  0.000011 11186 ./traces/hostname.rep
   * yes    91%   19405  0.148391   131 ./traces/login.rep
   * yes    88%     372  0.000053  6954 ./traces/ls.rep
     yes    28%      17  0.000000138894 ./traces/malloc-free.rep
     yes    34%      10  0.000000 96807 ./traces/malloc.rep
   * yes    86%    1494  0.001016  1470 ./traces/perl.rep
   * yes    96%    4800  0.009478   506 ./traces/random.rep
   * yes    79%     147  0.000014 10465 ./traces/rm.rep
     yes    89%      12  0.000000109714 ./traces/short2.rep
   * yes    56%   57716  1.743519    33 ./traces/boat.rep
   * yes    63%     200  0.000005 38285 ./traces/lrucd.rep
   * yes    86%  100000  0.007144 13998 ./traces/alaska.rep
   * yes    89%     200  0.000006 31557 ./traces/nlydf.rep
   * yes    86%     200  0.000006 35036 ./traces/qyqyc.rep
   * yes    68%     200  0.000005 42730 ./traces/rulsr.rep
  16        83%  214772  1.930179   111
  
  Perf index = 44 (util) + 0 (thru) = 44/100
  ```

  总的来说，分数很低的主要原因是数据结构 Implicit free list 本身过于简单的问题。

  我们当然可以综合 fit strategy，比如结合 next fit 和 best fit，即在上一次 alloc 得到的块之后的 n 个块中找那个最好的块，不过如果不引入新的 strategy，那么 util 不会超过 best fit 的 44，thru 也不会超过 next fit 的 17，最后最多 60 分，果断放弃！

* 接下来把 implicit free list 修改成 explicit list

  ```shell
  # mm-EFL.c
  # first fit
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    88%    4805  0.000158 30382 ./traces/amptjp.rep
   * yes    92%    5032  0.000077 65485 ./traces/cccp.rep
   * yes    66%   14400  0.000082176128 ./traces/coalescing-bal.rep
     yes    96%      15  0.000000117551 ./traces/corners.rep
   * yes    94%    5683  0.000243 23390 ./traces/cp-decl.rep
   * yes    75%     118  0.000001155001 ./traces/hostname.rep
   * yes    85%   19405  0.000141137650 ./traces/login.rep
   * yes    77%     372  0.000002152290 ./traces/ls.rep
     yes    28%      17  0.000000146149 ./traces/malloc-free.rep
     yes    34%      10  0.000000137143 ./traces/malloc.rep
   * yes    77%    1494  0.000010150339 ./traces/perl.rep
   * yes    87%    4800  0.000357 13460 ./traces/random.rep
   * yes    79%     147  0.000001149202 ./traces/rm.rep
     yes    89%      12  0.000000102400 ./traces/short2.rep
   * yes    44%   57716  0.000602 95834 ./traces/boat.rep
   * yes    63%     200  0.000001171812 ./traces/lrucd.rep
   * yes    77%  100000  0.001527 65484 ./traces/alaska.rep
   * yes    76%     200  0.000001165043 ./traces/nlydf.rep
   * yes    57%     200  0.000001170037 ./traces/qyqyc.rep
   * yes    68%     200  0.000001172326 ./traces/rulsr.rep
  16        75%  214772  0.003205 67001
  
  Perf index = 29 (util) + 37 (thru) = 66/100
  
  # best fit
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.000091 52836 ./traces/amptjp.rep
   * yes    99%    5032  0.000119 42411 ./traces/cccp.rep
   * yes    66%   14400  0.000083174022 ./traces/coalescing-bal.rep
     yes    96%      15  0.000000113684 ./traces/corners.rep
   * yes    99%    5683  0.000121 46815 ./traces/cp-decl.rep
   * yes    75%     118  0.000001113754 ./traces/hostname.rep
   * yes    85%   19405  0.000149130039 ./traces/login.rep
   * yes    77%     372  0.000003139002 ./traces/ls.rep
     yes    28%      17  0.000000138893 ./traces/malloc-free.rep
     yes    34%      10  0.000000128000 ./traces/malloc.rep
   * yes    77%    1494  0.000011137281 ./traces/perl.rep
   * yes    96%    4800  0.002415  1988 ./traces/random.rep
   * yes    79%     147  0.000001135367 ./traces/rm.rep
     yes    89%      12  0.000000 81318 ./traces/short2.rep
   * yes    44%   57716  0.328486   176 ./traces/boat.rep
   * yes    63%     200  0.000001137061 ./traces/lrucd.rep
   * yes    86%  100000  0.003481 28729 ./traces/alaska.rep
   * yes    89%     200  0.000001136817 ./traces/nlydf.rep
   * yes    85%     200  0.000002111845 ./traces/qyqyc.rep
   * yes    68%     200  0.000001154735 ./traces/rulsr.rep
  16        80%  214772  0.334966   641
  
  Perf index = 39 (util) + 1 (thru) = 40/100
  
  # first fit + best fit
  # MAX_SEARCH_FREE_BLOCK 20
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.000103 46693 ./traces/amptjp.rep
   * yes    99%    5032  0.000129 38934 ./traces/cccp.rep
   * yes    66%   14400  0.000085170018 ./traces/coalescing-bal.rep
     yes    96%      15  0.000000111484 ./traces/corners.rep
   * yes    99%    5683  0.000122 46631 ./traces/cp-decl.rep
   * yes    75%     118  0.000001130833 ./traces/hostname.rep
   * yes    85%   19405  0.000152127373 ./traces/login.rep
   * yes    77%     372  0.000003135873 ./traces/ls.rep
     yes    28%      17  0.000000128000 ./traces/malloc-free.rep
     yes    34%      10  0.000000122553 ./traces/malloc.rep
   * yes    77%    1494  0.000011131150 ./traces/perl.rep
   * yes    94%    4800  0.000431 11148 ./traces/random.rep
   * yes    79%     147  0.000001133552 ./traces/rm.rep
     yes    89%      12  0.000000 98743 ./traces/short2.rep
   * yes    44%   57716  0.001063 54293 ./traces/boat.rep
   * yes    63%     200  0.000002122749 ./traces/lrucd.rep
   * yes    86%  100000  0.003517 28435 ./traces/alaska.rep
   * yes    89%     200  0.000002125833 ./traces/nlydf.rep
   * yes    85%     200  0.000002114342 ./traces/qyqyc.rep
   * yes    68%     200  0.000001137799 ./traces/rulsr.rep
  16        80%  214772  0.005624 38188
  
  Perf index = 39 (util) + 37 (thru) = 76/100
  ```

  相比之前的 Implicit free list，explicit free list 在吞吐量方面有了很好的改善，最后使用的 best fit + first fit 得到的结果中，吞吐量已经满分了，而空间利用率也和 best fit 差不多，基本达到了简单综合几种分配策略可以达到的上限。

  接下来要做的，就是要在不牺牲过多吞吐量的前提下，尽可能提高空间利用率。先实现 Segregate free list，再实现其他的优化。
