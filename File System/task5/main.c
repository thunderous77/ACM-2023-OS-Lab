#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
int main() {

  pid_t pid;
  // OPEN FILES
  int fd;
  fd = open("test.txt", O_RDWR | O_CREAT | O_TRUNC);
  if (fd == -1) {
    fprintf(stderr, "fail on open %s\n", "test.txt");
    return -1;
  }
  // write 'hello fcntl!' to file
  write(fd, "hello fcntl!", 12);

  // DUPLICATE FD
  int fd_shadow = fcntl(fd, F_DUPFD);

  pid = fork();

  if (pid < 0) {
    // FAILS
    printf("error in fork");
    return 1;
  }

  struct flock fl;

  if (pid > 0) {
    // PARENT PROCESS
    // set the lock
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fcntl(fd, F_SETLK, &fl);

    // append 'b'
    write(fd, "b", 1);

    // unlock
    fl.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &fl);

    sleep(3);

    // printf("%s", str); the feedback should be 'hello fcntl!ba'

    exit(0);

  } else {
    // CHILD PROCESS
    sleep(2);
    // get the lock
    fcntl(fd_shadow, F_GETLK, &fl);

    // append 'a'
    write(fd_shadow, "a", 1);

    exit(0);
  }
  close(fd);
  return 0;
}