CC := gcc

CFLAGS  := -g3 -Wall -O0 -MMD -MP -I../ -DVER=\"test\"
LDFLAGS := -lpthread

SRCS := $(wildcard *.c ../*.c)
DEPS := $(SRCS:.c=.c.d)

all: test-log test-set test-worker test-http

test-log: test-log.c.o ../log.c.o
	$(CC) -o $@ $^ $(LDFLAGS)
test-set: test-set.c.o ../set.c.o
	$(CC) -o $@ $^ $(LDFLAGS)
test-worker: test-worker.c.o \
	../worker.c.o ../log.c.o ../common.c.o ../connection.c.o ../set.c.o ../http.c.o \
	../ring-buffer.c.o
	$(CC) -o $@ $^ $(LDFLAGS)
test-http: test-http.c.o \
	../worker.c.o ../log.c.o ../common.c.o ../connection.c.o ../set.c.o ../http.c.o \
	../ring-buffer.c.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.c.o: %.c
	$(CC) -o $@ -c $^ $(CFLAGS)
#-include $(DEPS)

clean:
	$(RM) *.o test-log test-set test-worker test-http
distclean: clean
	$(RM) *.d

.PHONY: clean all
