CFLAGS += -Wall -Wextra -Werror -pedantic -std=gnu99 -g
#CFLAGS += -DDEBUG ## enable for more debug logging to syslog

all: pnt-lut test

pnt-lut: pnt-lut.c
	$(CC) $^ $(CFLAGS) -o $@

test: test.c
	$(CC) $^ $(CFLAGS) -o $@

format:
	clang-format -i pnt-lut.c test.c

clean:
	rm -f pnt-lut test

.PHONY: clean all format
