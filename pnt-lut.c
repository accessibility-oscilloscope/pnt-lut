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

#define TABLET_MAX_X 21600
#define TABLET_MAX_Y 13500

int better_read(int fd, void *buf, int amount) {
  int amt_read = 0;
  char *charbuf = (char *)buf;

  while (amt_read < amount) {
    int last_read = read(fd, charbuf + amt_read, amount - amt_read);
    amt_read += last_read;
  }

  return amt_read;
}

int main(int ac, char *av[]) {
  assert(ac == 4 && "usage: ./pnt-lut $PGM_FIFO $PNT_FIFO $OUTPUT_FIFO");
  openlog(NULL, LOG_PERROR, LOG_USER);

  const char *pgm_fifo = av[1];
  const char *pnt_fifo = av[2];
  const char *output_fifo = av[3];

  uint8_t *global = mmap(NULL, PGM_SIZE, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  memset(global, 0, PGM_SIZE);

  if (fork() == 0) {
    // parent process
    mkfifo(pgm_fifo, 0666);
    const int fd = open(pgm_fifo, O_RDONLY);
    for (;;) {
      int len_read;
      len_read = better_read(fd, global, PGM_SIZE);
      syslog(LOG_INFO, "read PGM %d", len_read);
    }
    close(fd);
    syslog(LOG_INFO, "parent: unreachable!\n");
  } else {
    // child process
    mkfifo(pnt_fifo, 0666);
    const int fd = open(pnt_fifo, O_RDONLY);
    const int fd_out = open(output_fifo, O_WRONLY);
    int pnt[2];
    for (;;) {
      const int rv = read(fd, pnt, sizeof(pnt));
      if (rv != sizeof(pnt)) {
        if (rv != 0) {
          syslog(LOG_INFO, "point read failed, rv=%d\n", rv);
        }
        continue;
      }

      int tablet_x = pnt[0], tablet_y = pnt[1];

      float pgm_x = PGM_W - tablet_y*PGM_W/TABLET_MAX_Y;

      float pgm_y = tablet_x*PGM_H/TABLET_MAX_X;

#ifdef DEBUG
      syslog(LOG_DEBUG, "tablet x is %d", tablet_x);
      syslog(LOG_DEBUG, "tablet y is %d", tablet_y);
      syslog(LOG_DEBUG, "calculated pgm_x is %f", pgm_x);
      syslog(LOG_DEBUG, "calculated pgm_y is %f", pgm_y);
#endif

      // output to haptic driver pipe
      uint8_t output[2];
      output[0] = 0; // real-time playback mode
      // offset 15
      // + row size * number of rows (y)
      // + distance into row (x)
      if (tablet_x == 0 && tablet_y == 0) { // pen lifted off the tablet
        output[1] = 0;
      }
      else {
        output[1] = global[15 + (int)pgm_y * PGM_W + (int)pgm_x]; // 0-255 strength of vibration
      }
#ifdef DEBUG
      syslog(LOG_DEBUG, "output is %x", output[1]);
#endif
      write(fd_out, output, 2);
    }
    syslog(LOG_INFO, "child: unreachable!\n");
    close(fd);
    close(fd_out);
  }

  munmap(global, sizeof(*global));
  return 0;
}
