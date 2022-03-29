#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
  assert(argc == 2);
  int fd = open(argv[1], O_WRONLY);
  int point[2];
  srandom(time(NULL));
  while (1) {
    point[0] = random() % 21600; // tablet x val
    point[1] = random() % 13500; // tablet y val
    write(fd, &point[0], sizeof(int));
    sleep(1);
    write(fd, &point[1], sizeof(int));
    sleep(1);
  }
  close(fd);
}
