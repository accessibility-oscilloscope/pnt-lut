/*
 * implementation of IPC using forks with global shared memory.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// see
// [signal-compiler.c](https://github.com/accessibility-oscilloscope/signal-compiler/blob/main/signal-compiler.c)
// PGM_W * PGM_H + (15 byte header)
// verify with `ls -l test.pgm`
#define PGM_W 300
#define PGM_H 480
#define PGM_SIZE ((PGM_W) * (PGM_H)) + 15

int main(int ac, char *av[]) {
  assert(ac == 3 && "usage: ./gradient $PGM_FIFO $PNT_FIFO");

  const char *pgm_fifo = av[1];
  const char *pnt_fifo = av[2];

  unsigned char *global = mmap(NULL, PGM_SIZE, PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  memset(global, 0, PGM_SIZE);

  if (fork() == 0) {
    // parent process
    mkfifo(pgm_fifo, 0666);
    const int fd = open(pgm_fifo, O_RDONLY);
    // note that if size > /proc/sys/fs/pipe-max-size, then requires root access!
    fcntl(fd, F_SETPIPE_SZ, PGM_SIZE);
    for (;;) {
      int rval;
      while ((rval = read(fd, global, PGM_SIZE)) != PGM_SIZE) {
        if (rval != 0)
          printf("%d\n", rval);
      }
      printf("read PGM\n");
    }
    close(fd);
    printf("parent: unreachable!\n");
  } else {
    // child process
    mkfifo(pnt_fifo, 0666);
    for (;;) {
      for (int ii = 0; ii < 20; ii += 1)
        printf("%d ", global[ii]);
      printf("\n");
      sleep(1);
    }
    printf("child: unreachable!\n");
  }

  munmap(global, sizeof(*global));
  return 0;
}
