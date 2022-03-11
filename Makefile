CFLAGS += -Wall -Wextra -Werror -pedantic -std=gnu99 -g

all: pnt-lut test

pnt-lut: pnt-lut.c
	$(CC) $^ $(CFLAGS) -o $@

test: test.c
	$(CC) $^ $(CFLAGS) -o $@

.PHONY: clean all
clean:
	rm -f pnt-lut test
