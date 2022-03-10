#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv) {
  assert(argc == 2);
  int fd = open(argv[1], O_WRONLY);
  int point[2];
  srandom(time(NULL));
  while(1) {
    point[0] = random() % 21600; // tablet x val
    point[1] = random() % 13500; // tablet y val
    write(fd, point, sizeof(point));
    sleep(1);
  }
  close(fd);
}
