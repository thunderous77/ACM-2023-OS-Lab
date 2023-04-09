# Buddy and Malloc

## Buddy 内存分配

### 实现思路

* 每个大小的 buddy_page 都是一个单向链表，内存地址从小到大排列
  
* 对于 buddy_page 的 merge
  * return_pages 时，将 return 的 buddy_page 与前后相匹配（由同一个上一级 buddy_page split 而来）的 buddy_page 进行 merge，只 merge 一次
  
  * create_buddy_page 时，将新创建的 buddy_page 与前后相匹配的 buddy_page 进行 merge，递归向上 merge

## Malloc 动态存储分配器
