/*
 * implementation of IPC using forks with global shared memory.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
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

  uint8_t *global = mmap(NULL, PGM_SIZE, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  memset(global, 0, PGM_SIZE);

  if (fork() == 0) {
    // parent process
    mkfifo(pgm_fifo, 0666);
    const int fd = open(pgm_fifo, O_RDONLY);
    // note that if size > /proc/sys/fs/pipe-max-size, then requires root
    // access!
    fcntl(fd, F_SETPIPE_SZ, PGM_SIZE);
    for (;;) {
      while (read(fd, global, PGM_SIZE) != PGM_SIZE) {
      }
      printf("read PGM\n");
    }
    close(fd);
    printf("parent: unreachable!\n");
  } else {
    // child process
    mkfifo(pnt_fifo, 0666);
    const int fd = open(pnt_fifo, O_RDONLY);
    uint8_t pnt[2];
    for (;;) {
      const int rv = read(fd, pnt, sizeof(pnt));
      if (rv != sizeof(pnt))
        continue;

      // offset 15
      // + row size * number of rows (y)
      // + distance into row (x)
      uint8_t x = pnt[0], y = pnt[1];
      printf("%d\n", global[15 + y * PGM_W + x]);
    }
    printf("child: unreachable!\n");
    close(fd);
  }

  munmap(global, sizeof(*global));
  return 0;
}
