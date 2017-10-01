CC := gcc

CFLAGS  := -g3 -Wall -O0 -MMD -MP -I../
LDFLAGS :=

SRCS := $(wildcard *.c ../*.c)
DEPS := $(SRCS:.c=.c.d)

all: test-log

test-log: test-log.c.o ../log.c.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.c.o: %.c
	$(CC) -o $@ -c $^ $(CFLAGS)
-include $(DEPS)

clean:
	$(RM) *.o test-log
distclean: clean
	$(RM) *.d

.PHONY: clean all
