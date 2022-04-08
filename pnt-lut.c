/*
 * implementation of IPC using forks with global shared memory.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
#define PGM_W 480
#define PGM_H 300
#define PGM_SIZE ((PGM_W) * (PGM_H)) + 15

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

#define TABLET_MAX_X 21600
#define TABLET_MAX_Y 13500

// flag to write image to file
static volatile sig_atomic_t sig_rcvd = 0;

int better_read(int fd, void *buf, int amount)
{
  int amt_read = 0;
  char *charbuf = (char *)buf;

  while (amt_read < amount)
  {
    int last_read = read(fd, charbuf + amt_read, amount - amt_read);
    amt_read += last_read;
  }

  return amt_read;
}

void signal_handler(int signum)
{
  if (signum == 2)
  {
    sig_rcvd = 1;
  }
}

int main(int ac, char *av[])
{
  assert(ac == 4 && "usage: ./pnt-lut $PGM_FIFO $PNT_FIFO $OUTPUT_FIFO");
  openlog("pnt-lut", 0, LOG_USER);

  const char *pgm_fifo = av[1];
  const char *pnt_fifo = av[2];
  const char *output_fifo = av[3];

  uint8_t *global = mmap(NULL, PGM_SIZE, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  memset(global, 0, PGM_SIZE);

  if (fork() == 0)
  {
    // parent process
    const int fd = open(pgm_fifo, O_RDONLY);
    if (fd == -1)
    {
      syslog(LOG_ERR, "opening fifo %s failed", pgm_fifo);
      exit(-1);
    }
    syslog(LOG_ERR, "pnt-lut: parent initialized");

    for (;;)
    {
      int len_read;
      len_read = better_read(fd, global, PGM_SIZE);
      unsigned int width, height, maxval;
      sscanf((char *)global, "P5 %u %u %u", &width, &height, &maxval);
      syslog(LOG_INFO, "read PGM %d: width=%u, height=%u, maxval=%u", len_read,
             width, height, maxval);
      if (width != PGM_W || height != PGM_H)
      {
        // TODO: bail if bad? would need two arrays or to dynamically
        // read+discard sscanf or fgets could do it but it's ... not ideal
        syslog(LOG_ERR,
               "PGM INVALID! width=%u, height=%u, expected width=%u, "
               "height=%u. Lookups will be using invalid data!",
               width, height, PGM_W, PGM_H);
      }
    }
    close(fd);
    syslog(LOG_INFO, "parent: unreachable!\n");
  }
  else
  {
    // child process
    const int fd = open(pnt_fifo, O_RDONLY);
    const int fd_out = open(output_fifo, O_WRONLY);
    if (fd == -1)
    {
      syslog(LOG_ERR, "opening fifo %s failed", pgm_fifo);
      exit(-1);
    }
    if (fd_out == -1)
    {
      syslog(LOG_ERR, "opening fifo %s failed", output_fifo);
      exit(-1);
    }
    syslog(LOG_ERR, "pnt-lut: child initialized");

    int pnt[2];
    for (;;)
    {
      const int rv = better_read(fd, pnt, sizeof(pnt));
      if (rv != sizeof(pnt))
      {
        if (rv != 0)
        {
          syslog(LOG_INFO, "point read failed, rv=%d\n", rv);
        }
        continue;
      }

      int tablet_x = pnt[0], tablet_y = pnt[1];

      // floats avoid integer math problems (overflow or truncation)
      float pgm_x = tablet_x * PGM_W / TABLET_MAX_X;
      float pgm_y = tablet_y * PGM_H / TABLET_MAX_Y;

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
      if (tablet_x == 0 && tablet_y == 0)
      {
        // pen lifted off the tablet, disable output
        output[1] = 0;
      }
      else
      {
        // 0-255 strength of vibration
        // cast floats to ints here to use as indexes
        output[1] = global[15 + (int)pgm_y * PGM_W + (int)pgm_x];
      }
#ifdef DEBUG
      syslog(LOG_DEBUG, "output is 0x%x", output[1]);
#endif
      write(fd_out, output, 2);

      // write image to file if SIGINT is detected
      if (sig_rcvd)
      {
        int pptr = open("image.pgm", O_WRONLY);
        uint8_t pgm_buf[PGM_W][PGM_H] = {0};
        const char *pgm_init = "P5 " STR(PGM_W) " " STR(PGM_H) " " STR(PGM_DEPTH) " ";
        write(pptr, pgm_init, strlen(pgm_init));
        for (unsigned jj = 0; jj < PGM_H; jj++)
        {
          for (unsigned ii = 0; ii < PGM_W; ii++)
          {
            write(pptr, &pgm_buf[ii][jj], 1);
          }
        }
        syslog(LOG_INFO, "wrote PGM size %zu\n", sizeof(pgm_buf));
        close(pptr);
        sig_rcvd = 0;
      }
    }
    syslog(LOG_INFO, "child: unreachable!\n");
    close(fd);
    close(fd_out);
  }

  munmap(global, sizeof(*global));
  return 0;
}
