// compile with "gcc -Wall rbtree.c my_fuse.c `pkg-config fuse3 --cflags --libs`
// -o myfuse"
#include <asm-generic/errno-base.h>
#include <sys/stat.h>
#define FUSE_USE_VERSION 31

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "rbtree.h"
#include <pthread.h>
#include <stdlib.h>

#define MAX_NODES 1000

static struct options {
  char *filename;
  char *contents;
  int show_help;
} options[MAX_NODES];

#define OPTION(t, p)                                                           \
  { t, offsetof(struct options, p), 1 }

static const struct fuse_opt option_spec[] = {
    OPTION("--name=%s", filename), OPTION("--contents=%s", contents),
    OPTION("-h", show_help), OPTION("--help", show_help), FUSE_OPT_END};

// File inodes build on rbtree
struct rb_root root = RB_ROOT;

struct memfs_file {
  struct rb_node node;
  char *path;
  struct options *option;
  int dir_or_file; // 1: dir 2: file

  struct stat file_stat; // file attributes
};

// 返回一个字符串最后一个 '/' 后的字符串
const char *getSubstringAfterLastSlash(const char *str) {
  const char *lastSlash = strrchr(str, '/');
  if (lastSlash == NULL) {
    // 没有找到斜杠
    return str;
  } else {
    // 返回斜杠后的字符串
    return lastSlash + 1;
  }
}

// 如果 parent 是 path 的前缀，则返回 path 中 parent 之后的字符串
static inline const char *__is_parent(const char *parent, const char *path) {
  const char slash = '/';

  if (parent[1] == '\0' && parent[0] == '/' && path[0] == '/') {
    return path;
  }

  while (*parent != '\0' && *path != '\0' && *parent == *path) {
    ++parent, ++path;
  }
  return (*parent == '\0' && *path == slash) ? path : NULL;
}

// Reference: https://blog.csdn.net/stayneckwind2/article/details/82867062
// insert inode into rbtree
static int __insert(struct rb_root *root, struct memfs_file *pf) {
  struct rb_node **new = &(root->rb_node), *parent = NULL;

  while (*new) {
    struct memfs_file *this = container_of(*new, struct memfs_file, node);
    int result = strcmp(pf->path, this->path);
    parent = *new;
    if (result < 0)
      new = &((*new)->rb_left);
    else if (result > 0)
      new = &((*new)->rb_right);
    else
      return -1;
  }
  rb_link_node(&pf->node, parent, new);
  rb_insert_color(&pf->node, root);
  return 0;
}

// search inode in rbtree
static struct memfs_file *__search(struct rb_root *root, const char *path) {
  struct rb_node *node = root->rb_node;
  struct memfs_file *data = NULL;

  while (node) {
    data = container_of(node, struct memfs_file, node);
    int result = strcmp(path, data->path);

    if (result < 0)
      node = node->rb_left;
    else if (result > 0)
      node = node->rb_right;
    else
      return data;
  }
  return NULL;
}

// free inode
static void __free(struct memfs_file *pf) {
  if (pf->path)
    free(pf->path);
  if (pf->option)
    free(pf->option);
  free(pf);
}

// delete inode from rbtree
static int __delete(struct rb_root *root, const char *path) {
  int res = 0;
  struct memfs_file *pf = __search(root, path);
  if (!pf) {
    return -1;
  }
  rb_erase(&pf->node, root);
  __free(pf);
  return res;
}

static void *my_fuse_init(struct fuse_conn_info *conn,
                          struct fuse_config *cfg) {
  (void)conn;
  cfg->kernel_cache = 1;
  char *root_path = "/";
  struct options *new_option = malloc(sizeof(struct options));
  new_option->filename = strdup(getSubstringAfterLastSlash(root_path));
  new_option->contents = strdup("");
  struct memfs_file *new_node = malloc(sizeof(struct memfs_file));
  new_node->path = strdup(root_path);
  new_node->option = new_option;
  new_node->dir_or_file = 1;
  __insert(&root, new_node);
  return NULL;
}

// get file attributes
static int my_fuse_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi) {
  (void)fi;

  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else {
    struct memfs_file *pf = __search(&root, path);
    if (!pf) {
      return -ENOENT;
    }

    // memcpy(stbuf, &pf->file_stat, sizeof(struct stat));
    if (pf->dir_or_file == 2 && strcmp(path, pf->path) == 0) {
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = strlen(pf->option->contents);
    } else if (pf->dir_or_file == 1) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
    } else {
      return -ENOENT;
    }
  }

  return 0;
}

// check file access permissions
static int my_fuse_access(const char *path, int mask) { return 0; }

// read directory
static int my_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi,
                           enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;

  filler(buf, ".", NULL, 0, 0);
  if (strcmp(path, "/") != 0) {
    filler(buf, "..", NULL, 0, 0);
  }

  struct rb_node *node;
  struct memfs_file *data;

  struct memfs_file *parent = __search(&root, path);
  if (!parent) {
    return -ENOENT;
  }

  for (node = rb_next(&parent->node); node; node = rb_next(node)) {
    data = rb_entry(node, struct memfs_file, node);
    const char *child = __is_parent(path, data->path);

    if (!child) {
      break;
    }

    // only return children in first level, skip "/xxx/..."
    if (strchr(child + 1, '/')) {
      continue;
    }

    filler(buf, child + 1, NULL, 0, 0);
  }

  return 0;
}

// open file
static int my_fuse_open(const char *path, struct fuse_file_info *fi) {
  return 0;
}

// read data from an open file
static int my_fuse_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
  (void)fi;
  struct memfs_file *pf = __search(&root, path);
  if (!pf) {
    return -ENOENT;
  }

  size_t len = strlen(pf->option->contents);
  if (offset < len) {
    if (offset + size > len)
      size = len - offset;
    memcpy(buf, pf->option->contents + offset, size);
  } else
    size = 0;

  return size;
}

// write data to an open file
static int my_fuse_write(const char *path, const char *buf, size_t size,
                         off_t offset, struct fuse_file_info *fi) {
  (void)fi;
  struct memfs_file *pf = __search(&root, path);
  if (!pf) {
    return -ENOENT;
  }

  size_t len = strlen(pf->option->contents);
  if (offset + size > len) {
    pf->option->contents = realloc(pf->option->contents, offset + size + 1);
    if (!pf->option->contents) {
      return -ENOMEM;
    }
  }

  strcpy(pf->option->contents + offset, buf);
  pf->option->contents[offset + size] = '\0';
  return size;
}

// close an open file
static int my_fuse_release(const char *path, struct fuse_file_info *fi) {
  return 0;
}

// create a file node
static int my_fuse_mknod(const char *path, mode_t mode, dev_t rdev) {
  const struct memfs_file *pf = __search(&root, path);
  if (pf) {
    return -EEXIST;
  }
  struct options *new_option = malloc(sizeof(struct options));
  new_option->filename = strdup(getSubstringAfterLastSlash(path));
  new_option->contents = strdup("");
  struct memfs_file *new_node = malloc(sizeof(struct memfs_file));
  new_node->path = strdup(path);
  new_node->option = new_option;
  new_node->dir_or_file = 2;
  __insert(&root, new_node);
  return 0;
}

// remove a file
static int my_fuse_unlink(const char *path) {
  const struct memfs_file *pf = __search(&root, path);
  if (!pf) {
    return -ENOENT;
  }
  __delete(&root, path);
  return 0;
}

// create a directory
static int my_fuse_mkdir(const char *path, mode_t mode) {
  const struct memfs_file *pf = __search(&root, path);
  if (pf) {
    return -EEXIST;
  }

  struct options *new_option = malloc(sizeof(struct options));
  new_option->filename = strdup(getSubstringAfterLastSlash(path));
  new_option->contents = strdup("");
  struct memfs_file *new_node = malloc(sizeof(struct memfs_file));
  new_node->path = strdup(path);
  new_node->option = new_option;
  new_node->dir_or_file = 1;
  __insert(&root, new_node);
  return 0;
}

// remove a directory
static int my_fuse_rmdir(const char *path) {
  int res = 0;
  if ((res = __delete(&root, path)) < 0) {
    return -ENOENT;
  }
  return res;
}

// change the access and modification times of a file with nanosecond
static int my_fuse_utimes(const char *path, const struct timespec tv[2],
                          struct fuse_file_info *fi) {
  (void)fi;
  struct memfs_file *pf = __search(&root, path);
  if (!pf) {
    struct options *new_option = malloc(sizeof(struct options));
    new_option->filename = strdup(getSubstringAfterLastSlash(path));
    new_option->contents = strdup("");
    struct memfs_file *new_node = malloc(sizeof(struct memfs_file));
    new_node->path = strdup(path);
    new_node->option = new_option;
    new_node->dir_or_file = 2;
    __insert(&root, new_node);
  }

  utimensat(0, path, tv, AT_SYMLINK_NOFOLLOW);

  return 0;
}

static const struct fuse_operations my_fuse_oper = {
    .init = my_fuse_init,
    .getattr = my_fuse_getattr,
    .access = my_fuse_access,
    .readdir = my_fuse_readdir,
    .open = my_fuse_open,
    .read = my_fuse_read,
    .write = my_fuse_write,
    .release = my_fuse_release,
    .mknod = my_fuse_mknod,
    .unlink = my_fuse_unlink,
    .mkdir = my_fuse_mkdir,
    .rmdir = my_fuse_rmdir,
    .utimens = my_fuse_utimes,
};

int main(int argc, char *argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  // options.filename = strdup("hello");
  // options.contents = strdup("Hello World!\n");
  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
    return 1;

  ret = fuse_main(args.argc, args.argv, &my_fuse_oper, NULL);
  fuse_opt_free_args(&args);
  return ret;
}