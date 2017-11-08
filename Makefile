APP := yhttpd
VER := 0.1.0
PKG := $(APP)-$(VER)

CC := gcc
RM := rm -rf
MKDIR := mkdir -p

CFLAGS  := -DVER=\"$(VER)\" -MMD -MP
LDFLAGS := -lpthread

CFLAGS_DEBUG    := -g3 -O0 -DDEBUG -Wall -Wformat -fsanitize=address
LDFLAGS_DEBUG   := -fsanitize=address
CFLAGS_RELEASE  := -O2 -DNDEBUG
LDFLAGS_RELEASE :=

CFLAGS  += $(CFLAGS_DEBUG)
LDFLAGS += $(LDFLAGS_DEBUG)

SRCS := $(wildcard src/*.c)
DEPS := $(SRCS:.c=.c.d)
OBJS := $(SRCS:.c=.c.o)

$(APP): $(OBJS)
	@$(CC) -o $@ $^ $(LDFLAGS)
	@echo "GEN $@"
%.c.o: %.c
	@$(CC) -o $@ -c $^ $(CFLAGS)
	@echo "CC  $@"
-include $(DEPS)

test:
	@$(MAKE) -C tests
	@( cd tests; ./test.sh )

clean:
	@$(RM) $(OBJS) $(DEPS) $(APP)
	@echo "RM $(OBJS)"
	@echo "RM $(DEPS)"
	@echo "RM $(APP)"

distclean: clean

pkg:

tar:

.PHONY: test clean distclean pkg tar
