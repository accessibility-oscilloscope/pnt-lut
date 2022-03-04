BIN = pnt-lut
SRCS = $(wildcard *.c)

CFLAGS += -Wall -Wextra -Werror -pedantic -std=gnu99 -g

$(BIN): $(SRCS)
	$(CC) $^ $(CFLAGS) -o $@

.PHONY: clean
clean:
	rm -f $(BIN)
