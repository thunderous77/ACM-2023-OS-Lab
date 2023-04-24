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
    
    <img src="https://github.com/thunderous77/ACM-2023-OS-Lab/blob/main/Memory%20Management/doc/Implicit free list1.png" style="zoom:50%;" />
    
    这里 31 - 0 就是之前堆大小限制的优化
    
    由于 8 字节对齐，最后 3 位一定是 0，可以用来储存是否被 allocated 的信息
    
    ```c
    #define PACK(size, alloc) ((size) | (alloc))
    ```
    
    Payload 储存 allocated 的块中的内容，而 Padding 用来对齐
    
    前后都存 Block size 可以以常数时间查询块大小，从而以常数时间与前或后的块合并
    
    <img src="https://github.com/thunderous77/ACM-2023-OS-Lab/blob/main/Memory%20Management/doc/Implicit free list2.jpg" style="zoom:80%;" />
    
    Prologue block 和 Epilogue block 都是为了方便合并空闲块，而第一个则是为了对齐 8 byte
    
  * `Explicit free list`
  
    每个空闲块都记录了前一个空闲块和后一个空闲块
  
    遍历复杂度：O(块总数) $\to$ O(空闲块总数)
  
    <img src="https://github.com/thunderous77/ACM-2023-OS-Lab/blob/main/Memory%20Management/doc/Explicit free list1.png" alt="img" style="zoom:50%;" />
  
    <img src="https://github.com/thunderous77/ACM-2023-OS-Lab/blob/main/Memory%20Management/doc/Explicit free list2.jpg" style="zoom: 25%;" />
  
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

  本来应该接着实现 segregated free list，但是数据结构本身不能提高空间利用率上限，暂时先不写了。
  
* 突然看了一看测评数据，发现了一个惊人的优化--原来我一次申请的空间太多了，导致有的数据点没有用完。

  ```shell
  # mm-ELF.c
  # CHUNKSIZE (1 << 12) --> (1 << 8) 
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.000084 56924 ./traces/amptjp.rep
   * yes    99%    5032  0.000123 40909 ./traces/cccp.rep
   * yes    96%   14400  0.000085168647 ./traces/coalescing-bal.rep
     yes   100%      15  0.000000101647 ./traces/corners.rep
   * yes    99%    5683  0.000126 45087 ./traces/cp-decl.rep
   * yes    76%     118  0.000001116185 ./traces/hostname.rep
   * yes    85%   19405  0.000170113833 ./traces/login.rep
   * yes    85%     372  0.000003130773 ./traces/ls.rep
     yes    87%      17  0.000000123949 ./traces/malloc-free.rep
     yes    76%      10  0.000000103784 ./traces/malloc.rep
   * yes    79%    1494  0.000012120651 ./traces/perl.rep
   * yes    93%    4800  0.000389 12336 ./traces/random.rep
   * yes    88%     147  0.000001127903 ./traces/rm.rep
     yes    98%      12  0.000000102400 ./traces/short2.rep
   * yes    43%   57716  0.001606 35947 ./traces/boat.rep
   * yes    89%     200  0.000002120628 ./traces/lrucd.rep
   * yes    88%  100000  0.003649 27408 ./traces/alaska.rep
   * yes    95%     200  0.000002118397 ./traces/nlydf.rep
   * yes    89%     200  0.000002121391 ./traces/qyqyc.rep
   * yes    93%     200  0.000002126524 ./traces/rulsr.rep
  16        87%  214772  0.006257 34327
  
  Perf index = 52 (util) + 37 (thru) = 89/100
  ```

  换了一个 CHUNKSIZE 之后结果有了明显的提升。

* 突然想到了自己之前想的一个优化--小内存和大内存分别从 free block 的两侧分配，保证之后能合并成更大的，就简单的实现一下

  ```shell
  # mm-ELF2.c
  # void set_block() --> void *set_block_front() && void *set_block_behind()
  # MAX_FRONT_BLOCK_SIZE (1 << 6)
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.000112 42908 ./traces/amptjp.rep
   * yes    99%    5032  0.000130 38782 ./traces/cccp.rep
   * yes    96%   14400  0.000086167674 ./traces/coalescing-bal.rep
     yes   100%      15  0.000000101053 ./traces/corners.rep
   * yes    99%    5683  0.000147 38694 ./traces/cp-decl.rep
   * yes    76%     118  0.000001135529 ./traces/hostname.rep
   * yes    85%   19405  0.000243 79962 ./traces/login.rep
   * yes    85%     372  0.000003129901 ./traces/ls.rep
     yes    87%      17  0.000000129695 ./traces/malloc-free.rep
     yes    76%      10  0.000000100174 ./traces/malloc.rep
   * yes    78%    1494  0.000012123748 ./traces/perl.rep
   * yes    93%    4800  0.000405 11864 ./traces/random.rep
   * yes    88%     147  0.000001137121 ./traces/rm.rep
     yes    98%      12  0.000000114248 ./traces/short2.rep
   * yes    43%   57716  0.001392 41463 ./traces/boat.rep
   * yes    89%     200  0.000002129003 ./traces/lrucd.rep
   * yes    88%  100000  0.003544 28219 ./traces/alaska.rep
   * yes    95%     200  0.000001136412 ./traces/nlydf.rep
   * yes    89%     200  0.000002124878 ./traces/qyqyc.rep
   * yes    93%     200  0.000001133953 ./traces/rulsr.rep
  16        87%  214772  0.006080 35322
  
  Perf index = 52 (util) + 37 (thru) = 89/100
  ```

  调整中间的分界点，发现分数和之前基本没有区别。

* 看了一下空间利用率比较低的几个点，发现问题如下：

  数据点大量要求 malloc 小内存（16 byte 左右的），但是 Explicit free list 中一块内存额外需要 16 byte 来维护数据结构，这种代价显然是不能接受的。所以可以牺牲一定的吞吐量来提升空间利用率。
  
  具体来说，我把 free list 从双向链表改成了单向链表，一个块用于维护数据结构的内存从 16 byte 下降到了 12 byte
  
  缺点显然的，要 remove free block 时需要遍历 free list
  
  ```shell
  # mm-EFL3.c
  # free list: double linked list --> single linked list
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.000092 52505 ./traces/amptjp.rep
   * yes    99%    5032  0.000106 47491 ./traces/cccp.rep
   * yes    96%   14400  0.000087165247 ./traces/coalescing-bal.rep
     yes   100%      15  0.000000104096 ./traces/corners.rep
   * yes    99%    5683  0.000123 46119 ./traces/cp-decl.rep
   * yes    79%     118  0.000001130332 ./traces/hostname.rep
   * yes    88%   19405  0.000218 88991 ./traces/login.rep
   * yes    87%     372  0.000003133753 ./traces/ls.rep
     yes    87%      17  0.000000128000 ./traces/malloc-free.rep
     yes    77%      10  0.000000 90709 ./traces/malloc.rep
   * yes    82%    1494  0.000013114724 ./traces/perl.rep
   * yes    92%    4800  0.001395  3441 ./traces/random.rep
   * yes    90%     147  0.000001127422 ./traces/rm.rep
     yes    98%      12  0.000000 99453 ./traces/short2.rep
   * yes    55%   57716  0.001264 45671 ./traces/boat.rep
   * yes    89%     200  0.000002111358 ./traces/lrucd.rep
   * yes    85%  100000  0.004322 23139 ./traces/alaska.rep
   * yes    95%     200  0.000002114513 ./traces/nlydf.rep
   * yes    89%     200  0.000002114399 ./traces/qyqyc.rep
   * yes    93%     200  0.000002128285 ./traces/rulsr.rep
  16        89%  214772  0.007631 28144
  
  Perf index = 55 (util) + 37 (thru) = 92/100
  ```
  
  空间利用率比之前的高了一点，吞吐量虽然下降了，但是还是大于 25000，依然是满分的。优化成功！
  
* 又尝试了一下把物理内存上的链表改成单链表，删除了一个块的 footer，于是维护数据结构的内存从 12 byte 下降到了 8 byte
  
  缺点是每个 free block 只能和后面的块合并，我加了一个小优化，还判断它能不能和上一个 free block，即 free list 的头结点合并，但是感觉上还是会导致空间碎片化，不过还是试一试。
  
  ```shell
  # mm-EFL4.c
  # phycis list: double list -> single list
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.000119 40254 ./traces/amptjp.rep
   * yes    98%    5032  0.000139 36095 ./traces/cccp.rep
   * yes    49%   14400  0.000093155247 ./traces/coalescing-bal.rep
     yes   100%      15  0.000000 96000 ./traces/corners.rep
   * yes    99%    5683  0.000159 35705 ./traces/cp-decl.rep
   * yes    85%     118  0.000001135529 ./traces/hostname.rep
   * yes    91%   19405  0.000309 62813 ./traces/login.rep
   * yes    89%     372  0.000004 87244 ./traces/ls.rep
     yes    88%      17  0.000000136000 ./traces/malloc-free.rep
     yes    90%      10  0.000000107664 ./traces/malloc.rep
   * yes    86%    1494  0.000013113513 ./traces/perl.rep
   * yes    87%    4800  0.002450  1959 ./traces/random.rep
   * yes    93%     147  0.000001143148 ./traces/rm.rep
     yes    99%      12  0.000000151912 ./traces/short2.rep
   * yes    55%   57716  0.001302 44323 ./traces/boat.rep
   * yes    86%     200  0.000002106029 ./traces/lrucd.rep
   * yes    83%  100000  0.004104 24367 ./traces/alaska.rep
   * yes    96%     200  0.000002120628 ./traces/nlydf.rep
   * yes    90%     200  0.000002115315 ./traces/qyqyc.rep
   * yes    91%     200  0.000001139636 ./traces/rulsr.rep
  16        86%  214772  0.008702 24681
  
  Perf index = 50 (util) + 37 (thru) = 86/100
  ```
  
  和之前预料的差不多，空间利用率反而变差了，主要是存在碎片没有合并。优化失败！
  
  另外，我本来以为 boat.rep 中都是小内存的 malloc，这个版本会提升空间利用率，但是后来仔细看了一下数据，malloc 的内存大小是 12 byte 或 20 byte，加上 8 byte 对齐后，其实维护数据结构的内存大小不管是 8 byte 还是 12 byte，最后的块大小都是 24 byte 或 32 byte。
  
  感觉如果数据是 8 的整数 byte 的话，这个版本可以再调整一下。不过既然没有这种数据，就懒得改了。
  
* 突然得知本地测试的时候吞吐量是取了平均再算的，而 oj 上是每个点单独算的，吞吐量还是不够，所以还是写 segregated free list

  ```shell
  # mm-SFL.c
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.000103 46661 ./traces/amptjp.rep
   * yes    99%    5032  0.000113 44556 ./traces/cccp.rep
   * yes    95%   14400  0.000222 64793 ./traces/coalescing-bal.rep
     yes   100%      15  0.000000 58182 ./traces/corners.rep
   * yes    99%    5683  0.000137 41479 ./traces/cp-decl.rep
   * yes    78%     118  0.000002 66085 ./traces/hostname.rep
   * yes    88%   19405  0.000407 47715 ./traces/login.rep
   * yes    86%     372  0.000005 68305 ./traces/ls.rep
     yes    82%      17  0.000000 79610 ./traces/malloc-free.rep
     yes    73%      10  0.000000 53088 ./traces/malloc.rep
   * yes    82%    1494  0.000025 60079 ./traces/perl.rep
   * yes    95%    4800  0.000388 12386 ./traces/random.rep
   * yes    89%     147  0.000002 69432 ./traces/rm.rep
     yes    98%      12  0.000000 45775 ./traces/short2.rep
   * yes    55%   57716  0.000588 98231 ./traces/boat.rep
   * yes    88%     200  0.000002 94118 ./traces/lrucd.rep
   * yes    88%  100000  0.003826 26137 ./traces/alaska.rep
   * yes    95%     200  0.000003 71642 ./traces/nlydf.rep
   * yes    88%     200  0.000002 83147 ./traces/qyqyc.rep
   * yes    92%     200  0.000002 94233 ./traces/rulsr.rep
  16        89%  214772  0.005827 36860
  
  Perf index = 55 (util) + 37 (thru) = 92/100
  ```

* 然而，oj 上测完发现吞吐量和本地的差了很多，之前基于牺牲吞吐量的优化基本都作废了......

* 峰回路转，又有一个巧妙的优化：其实空闲链表的指针只有空闲块才有，而这些块中间那些 malloc 又 free 的空间（至少 8 byte ）是闲置的，因此可以把空闲链表的指针存到中间 malloc 之后分配给用户的空间，这样子每个块只需要 8 byte 来维护数据结构，同时空闲链表完全可以使用双链表。

  于是 reset 回了最初的 explicit free list 的链表版本，加上了这个优化，本地终于跑到了 95 分

  ```shell
  # mm-ELF5.c
  Results for mm malloc:
     valid  util   ops    secs     Kops  trace
   * yes    99%    4805  0.000099 48556 ./traces/amptjp.rep
   * yes   100%    5032  0.000131 38367 ./traces/cccp.rep
   * yes    97%   14400  0.000082174873 ./traces/coalescing-bal.rep
     yes   100%      15  0.000000113684 ./traces/corners.rep
   * yes    99%    5683  0.000137 41483 ./traces/cp-decl.rep
   * yes    85%     118  0.000001131086 ./traces/hostname.rep
   * yes    91%   19405  0.000186104380 ./traces/login.rep
   * yes    90%     372  0.000003134762 ./traces/ls.rep
     yes    88%      17  0.000000124739 ./traces/malloc-free.rep
     yes    90%      10  0.000000114059 ./traces/malloc.rep
   * yes    87%    1494  0.000012124518 ./traces/perl.rep
   * yes    93%    4800  0.000408 11765 ./traces/random.rep
   * yes    93%     147  0.000001123339 ./traces/rm.rep
     yes    99%      12  0.000000108000 ./traces/short2.rep
   * yes    55%   57716  0.001360 42449 ./traces/boat.rep
   * yes    90%     200  0.000002121776 ./traces/lrucd.rep
   * yes    92%  100000  0.003438 29089 ./traces/alaska.rep
   * yes    92%     200  0.000002114342 ./traces/nlydf.rep
   * yes    90%     200  0.000002123011 ./traces/qyqyc.rep
   * yes    91%     200  0.000001137799 ./traces/rulsr.rep
  16        90%  214772  0.005864 36625
  
  Perf index = 58 (util) + 37 (thru) = 95/100
  ```

  这里的分配策略是从 first fit 往后找 15 个空闲块

  到 oj 上测了一下，oj 上最好的参数是往后找 2 个空闲块（不然吞吐量就爆炸了），可以达到 1416 分（满分 1600），虽然和本地还是有不小的差距，但是尽力的

* 继续把这个优化加到 segregated free list 的版本，本地差不多（因为基于同样的分配策略，而本地吞吐量都是满分），但是上了 oj 吞吐量反而比之前小。想了一下，大概是 oj 对吞吐量卡的比较死，之前 explicit free list 的版本基本近似于 first fit，而 segregated free list 中判断一个块属于哪个大小的类虽然是 O(1) 的，但是常数较大，最后反而性能不佳。segregated free list 要发挥出优势，还是要在 best fit 遍历比较多的块的前提下，因为它排除了一些过小的空闲块。
### 结果

针对在线的 oj 测评机尝试下来比较好的结构：

总体块的连接采用 explicit free list，其中 physical list，free list 都采用双链表，寻找空闲块的方法使用 first fit + best fit，最后得分 1416 / 1600。