// compile with "gcc -Wall my_fuse.c `pkg-config fuse3 --cflags --libs` -o
// myfuse"
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

#include <pthread.h>
#include <stdlib.h>

#define MAX_NODES 1000

typedef struct options options;
typedef struct filenode filenode;
typedef struct memfs_file memfs_file;

struct options {
  char *filename;
  char *contents;
  int show_help;
} option[MAX_NODES];

#define OPTION(t, p)                                                           \
  { t, offsetof(options, p), 1 }

static const struct fuse_opt option_spec[] = {
    OPTION("--name=%s", filename), OPTION("--contents=%s", contents),
    OPTION("-h", show_help), OPTION("--help", show_help), FUSE_OPT_END};

filenode *head = NULL;

struct filenode {
  char *path;
  filenode *next;
  memfs_file *file;
};

struct memfs_file {
  char *path;
  options *option;
  int dir_or_file; // 1: dir 2: file

  struct stat file_stat; // file attributes
};

// create a filenode
filenode *create_filenode(char *path) {
  filenode *new_node = malloc(sizeof(filenode));
  new_node->path = strdup(path);
  new_node->next = NULL;
  return new_node;
}

// 返回一个字符串最后一个 '/' 后的字符串
const char *getSubstringAfterLastSlash(const char *str) {
  const char *lastSlash = strrchr(str, '/');
  if (lastSlash == NULL) {
    return str;
  } else {
    return lastSlash + 1;
  }
}

// 如果 parent 是 path 的前缀，则返回 path 中 parent 之后的字符串
static inline const char *__is_parent(const char *parent, const char *path) {
  const char slash = '/';

  if (parent[1] == '\0' && parent[0] == '/' && path[0] == '/') {
    if (path[1] != '\0')
      return path;
    else
      return NULL;
  }

  while (*parent != '\0' && *path != '\0' && *parent == *path) {
    ++parent, ++path;
  }
  return (*parent == '\0' && *path == slash) ? path : NULL;
}

static int __insert(memfs_file *pf) {
  filenode *new_node = create_filenode(pf->path);
  if (head != NULL) {
    new_node->file = pf;
    new_node->next = head->next;
    head->next = new_node;
  } else {
    new_node->file = pf;
    head = new_node;
  }
  return 0;
}

static memfs_file *__search(const char *path) {
  filenode *node = head;
  memfs_file *mfile = NULL;

  while (node) {
    mfile = node->file;
    int result = strcmp(path, mfile->path);

    if (result != 0)
      node = node->next;
    else
      return mfile;
  }
  return NULL;
}

static void __free(const char *path) {
  filenode *node = head;
  filenode *pre = NULL;
  while (node) {
    if (strcmp(path, node->path) == 0) {
      if (pre == NULL) {
        head = node->next;
      } else {
        pre->next = node->next;
      }
      free(node);
      break;
    }
    pre = node;
    node = node->next;
  }
}

static int __delete(const char *path) {
  memfs_file *pf = __search(path);
  if (!pf) {
    return -1;
  }
  __free(pf->path);
  return 0;
}

// check the permission
// permission: 0: read 1: write 2: execute
static inline int __check_permission(const char *path, int permission) {
  memfs_file *pf = __search(path);
  if (!pf) {
    return -ENOENT;
  }
  if (permission == 0) {
    if (pf->file_stat.st_mode & S_IRUSR) {
      return 0;
    } else {
      return -EACCES;
    }
  } else if (permission == 1) {
    if (pf->file_stat.st_mode & S_IWUSR) {
      return 0;
    } else {
      return -EACCES;
    }
  } else if (permission == 2) {
    if (pf->file_stat.st_mode & S_IXUSR) {
      return 0;
    } else {
      return -EACCES;
    }
  }
  return 0;
}

static void *my_fuse_init(struct fuse_conn_info *conn,
                          struct fuse_config *cfg) {
  (void)conn;
  cfg->kernel_cache = 1;
  char *root_path = "/";
  options *new_option = malloc(sizeof(options));
  new_option->filename = strdup(getSubstringAfterLastSlash(root_path));
  new_option->contents = strdup("");
  memfs_file *new_node = malloc(sizeof(memfs_file));
  new_node->path = strdup(root_path);
  new_node->option = new_option;
  new_node->dir_or_file = 1;
  new_node->file_stat.st_mode = S_IFDIR | 0755;
  new_node->file_stat.st_nlink = 2;
  new_node->file_stat.st_atime = time(NULL);
  new_node->file_stat.st_mtime = time(NULL);
  new_node->file_stat.st_ctime = time(NULL);
  __insert(new_node);
  return NULL;
}

// get file attributes
static int my_fuse_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi) {
  (void)fi;

  memset(stbuf, 0, sizeof(struct stat));

  memfs_file *pf = __search(path);
  if (!pf) {
    return -ENOENT;
  }

  // memcpy(stbuf, &pf->file_stat, sizeof(struct stat));
  stbuf->st_mode = pf->file_stat.st_mode;
  stbuf->st_nlink = pf->file_stat.st_nlink;
  stbuf->st_size = pf->file_stat.st_size;
  stbuf->st_atime = pf->file_stat.st_atime;
  stbuf->st_mtime = pf->file_stat.st_mtime;
  stbuf->st_ctime = pf->file_stat.st_ctime; 

  return 0;
}

// check file access permissions
static int my_fuse_access(const char *path, int mask) {
  if (__check_permission(path, mask) != 0) {
    return -EACCES;
  }
  return 0;
}

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

  memfs_file *data;

  memfs_file *parent = __search(path);
  if (!parent) {
    return -ENOENT;
  }

  filenode *fnode = head;
  for (fnode = head; fnode; fnode = fnode->next) {
    if (__check_permission(fnode->path, 0) != 0) {
      continue;
    }
    data = fnode->file;
    const char *child = __is_parent(path, data->path);

    // only return children in first level, skip "/xxx/..."
    if (child == NULL || strchr(child + 1, '/')) {
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
  memfs_file *pf = __search(path);
  if (!pf) {
    return -ENOENT;
  }
  if (__check_permission(path, 0) != 0) {
    return -EACCES;
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
  memfs_file *pf = __search(path);
  if (!pf) {
    return -ENOENT;
  }
  if (__check_permission(path, 1) != 0) {
    return -EACCES;
  }

  // write to the original file
  size_t len = strlen(pf->option->contents);
  char *store = malloc(sizeof(char) * len);
  strcpy(store, pf->option->contents);
  pf->option->contents = malloc(sizeof(char) * (len + size + 1));

  strcpy(pf->option->contents, store);
  strcat(pf->option->contents, buf);
  pf->option->contents[len + size] = '\0';
  pf->file_stat.st_size = len + size + 1;

  // write to another file in order to support chatting
  // example: echo "Hello" > bot2/bot1, write "Hello" to bot1/bot2
  char *tmp;
  if ((tmp = strchr(path + 1, '/'))) {
    char *new_path = malloc(sizeof(char) * strlen(path));
    strcpy(new_path, tmp);
    strncat(new_path, path, tmp - path);

    memfs_file *pf2 = __search(new_path);

    if (!pf2) {
      return -ENOENT;
    }

    size_t len2 = strlen(pf2->option->contents);
    char *store2 = malloc(sizeof(char) * len2);
    strcpy(store2, pf2->option->contents);
    pf2->option->contents = malloc(sizeof(char) * (len2 + size + 1));

    strcpy(pf2->option->contents, store2);
    strcat(pf2->option->contents, buf);
    pf2->option->contents[len2 + size] = '\0';
    pf2->file_stat.st_size = len2 + size + 1;
  }
  return size;
}

// close an open file
static int my_fuse_release(const char *path, struct fuse_file_info *fi) {
  return 0;
}

// create a file node
static int my_fuse_mknod(const char *path, mode_t mode, dev_t rdev) {
  memfs_file *pf = __search(path);
  if (pf) {
    return -EEXIST;
  }

  // create the original file
  options *new_option = malloc(sizeof(options));
  new_option->filename = strdup(getSubstringAfterLastSlash(path));
  new_option->contents = strdup("");
  memfs_file *new_node = malloc(sizeof(memfs_file));
  new_node->path = strdup(path);
  new_node->option = new_option;
  new_node->dir_or_file = 2;
  new_node->file_stat.st_mode = S_IFREG | mode;
  new_node->file_stat.st_nlink = 1;
  new_node->file_stat.st_atime = time(NULL);
  new_node->file_stat.st_mtime = time(NULL);
  new_node->file_stat.st_ctime = time(NULL);
  __insert(new_node);

  // create another file in order to support chatting
  char *tmp;
  if ((tmp = strchr(path + 1, '/'))) {
    char *new_path = malloc(sizeof(char) * strlen(path));
    strcpy(new_path, tmp);
    strncat(new_path, path, tmp - path);

    memfs_file *new_pf = __search(new_path);

    if (new_pf) {
      return -EEXIST;
    }

    options *new_option = malloc(sizeof(options));
    new_option->filename = strdup(getSubstringAfterLastSlash(new_path));
    new_option->contents = strdup("");
    memfs_file *new_node = malloc(sizeof(memfs_file));
    new_node->path = strdup(new_path);
    new_node->option = new_option;
    new_node->dir_or_file = 2;
    new_node->file_stat.st_mode = S_IFREG | mode;
    new_node->file_stat.st_nlink = 1;
    new_node->file_stat.st_atime = time(NULL);
    new_node->file_stat.st_mtime = time(NULL);
    new_node->file_stat.st_ctime = time(NULL);

    __insert(new_node);
  }
  return 0;
}

// remove a file
static int my_fuse_unlink(const char *path) {
  const memfs_file *pf = __search(path);
  if (!pf) {
    return -ENOENT;
  }
  __delete(path);
  return 0;
}

// create a directory
static int my_fuse_mkdir(const char *path, mode_t mode) {
  const memfs_file *pf = __search(path);
  if (pf) {
    return -EEXIST;
  }

  options *new_option = malloc(sizeof(options));
  new_option->filename = strdup(getSubstringAfterLastSlash(path));
  new_option->contents = strdup("");
  memfs_file *new_node = malloc(sizeof(memfs_file));
  new_node->path = strdup(path);
  new_node->option = new_option;
  new_node->dir_or_file = 1;
  new_node->file_stat.st_mode = S_IFDIR | mode;
  new_node->file_stat.st_nlink = 2;
  new_node->file_stat.st_atime = time(NULL);
  new_node->file_stat.st_mtime = time(NULL);
  new_node->file_stat.st_ctime = time(NULL);
  __insert(new_node);
  return 0;
}

// remove a directory
static int my_fuse_rmdir(const char *path) {
  int res = 0;
  if ((res = __delete(path)) < 0) {
    return -ENOENT;
  }
  return res;
}

// create a file or modify the time of a file
static int my_fuse_utimes(const char *path, const struct timespec tv[2],
                          struct fuse_file_info *fi) {
  (void)fi;
  memfs_file *pf = __search(path);
  if (!pf) {
    options *new_option = malloc(sizeof(options));
    new_option->filename = strdup(getSubstringAfterLastSlash(path));
    new_option->contents = strdup("");
    memfs_file *new_node = malloc(sizeof(memfs_file));
    new_node->path = strdup(path);
    new_node->option = new_option;
    new_node->dir_or_file = 2;
    new_node->file_stat.st_mode = S_IFREG | 0644;
    new_node->file_stat.st_nlink = 1;
    new_node->file_stat.st_atime = time(NULL);
    new_node->file_stat.st_mtime = time(NULL);
    new_node->file_stat.st_ctime = time(NULL);
    __insert(new_node);
  }

  // utimensat(0, path, tv, AT_SYMLINK_NOFOLLOW);
  time_t now = time(NULL);
  pf->file_stat.st_atime = now;
  pf->file_stat.st_ctime = now;
  pf->file_stat.st_mtime = now;

  return 0;
}

static int my_fuse_chmod(const char *path, mode_t mode,
                         struct fuse_file_info *fi) {
  (void)fi;
  memfs_file *pf = __search(path);
  if (pf) {
    if (pf->dir_or_file == 1)
      pf->file_stat.st_mode = S_IFDIR | mode;
    else if (pf->dir_or_file == 2)
      pf->file_stat.st_mode = S_IFREG | mode;
  }

  return 0;
}

static const struct fuse_operations my_fuse_oper = {.init = my_fuse_init,
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
                                                    .chmod = my_fuse_chmod};

int main(int argc, char *argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  // options.filename = strdup("hello");
  // options.contents = strdup("Hello World!\n");
  if (fuse_opt_parse(&args, &option, option_spec, NULL) == -1)
    return 1;

  ret = fuse_main(args.argc, args.argv, &my_fuse_oper, NULL);
  fuse_opt_free_args(&args);
  return ret;
}