
#ifndef OS_MM_H
#define OS_MM_H
#define MAX_ERRNO 4095
#define NULL ((void *)0)

#define OK 0
#define EINVAL 22 /* Invalid argument */
#define ENOSPC 28 /* No page left */
#define MAXRANK (16)
#define TESTSIZE (128)
#define MAXRANK0PAGE (TESTSIZE * 1024 / 4)

#define IS_ERR_VALUE(x) ((x) >= (unsigned long)-MAX_ERRNO)
static inline void *ERR_PTR(long error) { return (void *)error; }
static inline long PTR_ERR(const void *ptr) { return (long)ptr; }
static inline long IS_ERR(const void *ptr) {
  return IS_ERR_VALUE((unsigned long)ptr);
}

int pow2f(int n);

typedef struct buddy_page buddy_page;

// mem: [start_addr, end_addr)
// a linked list of buddy_page ordered by start_addr in ascending order
struct buddy_page {
  int size;
  void *start_addr;
  void *end_addr;
  int allocated;
  struct buddy_page *next;
};

void create_buddy_page(int rank, void *start_addr, int allocated);
void insert_page(int rank, buddy_page *new_page);
void remove_page(int rank, buddy_page *pre_page);

int init_page(void *p, int pgcount);
void *alloc_pages(int rank);
int return_pages(void *p);
int query_ranks(void *p);
int query_page_counts(int rank);

#endif