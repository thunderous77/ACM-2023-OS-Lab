#include "buddy.h"
#include <stdlib.h>

int page_count[MAXRANK] = {0};
int alloc_page_count[MAXRANK] = {0};
struct buddy_page *pages[MAXRANK];
void *start_addr, *end_addr;

int pow2f(int n) {
  if (n <= 0)
    return 1;
  return 2 * pow2f(n - 1);
}

void insert_page(int rank, buddy_page *new_page) {
  if (pages[rank] == NULL) {
    pages[rank] = new_page;
  } else {
    buddy_page *p = pages[rank];
    while (p->next != NULL && p->next->start_addr < new_page->start_addr) {
      p = p->next;
    }
    if (new_page->allocated == 0) {
      if ((unsigned long long)(p->start_addr - start_addr) %
                  (pow2f(rank + 1) * 4 * 1024) ==
              0 &&
          p != pages[rank] && p->end_addr == new_page->start_addr &&
          p->allocated == 0) {
        buddy_page *pre_page = pages[rank];
        while (pre_page->next != p)
          pre_page = pre_page->next;
        remove_page(rank, pre_page);
        create_buddy_page(rank + 1, p->start_addr, 0);
        return;
      } else if (p->next != NULL && p->next->start_addr == new_page->end_addr &&
                 p->next->allocated == 0) {
        remove_page(rank, p);
        create_buddy_page(rank + 1, new_page->start_addr, 0);
        return;
      }
    }
    new_page->next = p->next;
    p->next = new_page;
    page_count[rank]++;
    if (new_page->allocated == 0)
      alloc_page_count[rank]++;
  }
}

void remove_page(int rank, buddy_page *pre_page) {
  page_count[rank]--;
  if (pre_page->next->allocated == 0)
    alloc_page_count[rank]--;
  pre_page->next = pre_page->next->next;
}

buddy_page *search_alloc_page(int rank) {
  buddy_page *p = pages[rank]->next;
  while (p != NULL && p->allocated == 0) {
    p = p->next;
  }
  return p;
}

void create_buddy_page(int rank, void *start_addr, int allocated) {
  buddy_page *new_page = (buddy_page *)malloc(sizeof(buddy_page));
  new_page->size = pow2f(rank);
  new_page->start_addr = start_addr;
  new_page->end_addr = start_addr + pow2f(rank) * 1024 * 4;
  new_page->next = NULL;
  new_page->allocated = allocated;
  insert_page(rank, new_page);
}

int init_page(void *p, int pgcount) {
  int tmp[MAXRANK];
  start_addr = p;
  end_addr = p + TESTSIZE * 1024 * 1024;
  for (int i = 0; i < MAXRANK; i++) {
    pages[i] = NULL;
    create_buddy_page(i, NULL, 0);
  }
  for (int i = 0; i < MAXRANK; i++) {
    tmp[i] = pgcount % 2;
    pgcount /= 2;
  }
  for (int i = 0; i < MAXRANK; i++) {
    if (tmp[i] == 1) {
      create_buddy_page(i, p, 0);
    }
  }
  return OK;
}

void *alloc_pages(int rank) {
  int i;
  void *alloc_addr;
  if (rank > MAXRANK || rank <= 0)
    return (void *)(-EINVAL);
  if (alloc_page_count[rank - 1] >= 1) {
    buddy_page *ret_page = pages[rank - 1]->next;
    while (ret_page != NULL && ret_page->allocated == 1) {
      ret_page = ret_page->next;
    }
    ret_page->allocated = 1;
    alloc_page_count[rank - 1]--;
    return ret_page->start_addr;
  } else {
    for (i = rank; i < MAXRANK; i++) {
      if (alloc_page_count[i] >= 1) {
        buddy_page *pre_page = pages[i];
        while (pre_page->next != NULL && pre_page->next->allocated == 1) {
          pre_page = pre_page->next;
        }
        buddy_page *old_page = pages[i]->next;
        alloc_addr = pre_page->next->start_addr;
        remove_page(i, pre_page);
        create_buddy_page(rank - 1, old_page->start_addr, 1);
        for (int j = i - 1; j >= rank - 1; j--) {
          create_buddy_page(j, old_page->start_addr + pow2f(j) * 1024 * 4, 0);
        }
        free(old_page);
        break;
      }
    }
  }
  if (i == MAXRANK)
    return (void *)(-ENOSPC);
  else
    return alloc_addr;
}

int return_pages(void *p) {
  if (p < start_addr || p >= end_addr)
    return -EINVAL;
  for (int rank = 0; rank < MAXRANK; rank++) {
    // p2 -> p1 -> return_page
    buddy_page *p1 = pages[rank], *p2 = pages[rank];
    while (p1->next != NULL && p1->next->start_addr != p) {
      p1 = p1->next;
    }
    if (p1->next == NULL)
      continue;
    if ((unsigned long long)(p1->start_addr - start_addr) %
                (pow2f(rank + 1) * 4 * 1024) ==
            0 &&
        p1 != pages[rank] && p1->allocated == 0) {
      create_buddy_page(rank + 1, p1->start_addr, 0);
      remove_page(rank, p1);
      while (p2->next != p1) {
        p2 = p2->next;
      }
      remove_page(rank, p2);
      return OK;
    } else if (p1->next->next != NULL && p1->next->next->allocated == 0) {
      create_buddy_page(rank + 1, p1->next->start_addr, 0);
      remove_page(rank, p1->next);
      remove_page(rank, p1);
      return OK;
    } else {
      p1->next->allocated = 0;
      alloc_page_count[rank]++;
      return OK;
    }
  }
  return -EINVAL;
}

int query_ranks(void *p) {
  if (p < start_addr || p >= end_addr)
    return -EINVAL;
  for (int rank = 0; rank < MAXRANK; rank++) {
    buddy_page *p1 = pages[rank]->next;
    while (p1 != NULL && !(p1->start_addr <= p && p1->end_addr > p)) {
      p1 = p1->next;
    }
    if (p1 == NULL)
      continue;
    else
      return rank + 1;
  }
  return -EINVAL;
}

int query_page_counts(int rank) {
  if (rank > MAXRANK || rank <= 0)
    return -EINVAL;
  else
    return alloc_page_count[rank - 1];
}
void free_page() {
  for (int i = 0; i < MAXRANK; i++) {
    buddy_page *p = pages[i];
    while (p != NULL) {
      buddy_page *tmp = p;
      p = p->next;
      free(tmp);
    }
  }
}

void print_page() {
  for (int i = 0; i < MAXRANK; i++) {
    printf("rank: %d, page_count: %d, alloc_page_count: %d\n", i, page_count[i],
           alloc_page_count[i]);
  }
  for (int i = 0; i < MAXRANK; i++) {
    buddy_page *p = pages[i]->next;
    while (p != NULL) {
      printf("rank: %d, start_addr: %p, end_addr: %p\n", i, p->start_addr,
             p->end_addr);
      p = p->next;
    }
  }
}