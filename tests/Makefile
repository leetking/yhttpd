CC := gcc
VER := test

CFLAGS  := -g3 -Wall -O0 -MMD -MP -I../ -DVER=\"$(VER)\"
LDFLAGS := -lpthread

SRCS := $(filter-out ../main.c, $(wildcard ../*.c))
OBJS := $(SRCS:.c=.c.o)
DEPS := $(SRCS:.c=.c.d)

all: test-log test-set test-worker test-http

test-log: test-log.c.o ../log.c.o
	@$(CC) -o $@ $^ $(LDFLAGS)
	@echo "GEN $@"
test-set: test-set.c.o ../set.c.o
	@$(CC) -o $@ $^ $(LDFLAGS)
	@echo "GEN $@"
test-worker: test-worker.c.o $(OBJS)
	@$(CC) -o $@ $^ $(LDFLAGS)
	@echo "GEN $@"
test-http: test-http.c.o $(OBJS)
	@$(CC) -o $@ $^ $(LDFLAGS)
	@echo "GEN $@"

%.c.o: %.c
	@$(CC) -o $@ -c $^ $(CFLAGS)
	@echo "CC  $@"
-include $(DEPS)

clean:
	@$(RM) $(OBJS) *.o test-log test-set test-worker test-http
	@echo "RM  $(OBJS)"
	@echo "RM  test-log"
	@echo "RM  test-set"
	@echo "RM  test-worker"
	@echo "RM  test-http"
distclean: clean
	@$(RM) $(DEPS)
	@echo "RM  $(DEPS)"

.PHONY: clean all
