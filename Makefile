APP := yhttpd
VER := 0.1.0
PKG := $(APP)-$(VER)

CC := gcc
RM := rm -rf
MKDIR := mkdir -p

CFLAGS  := -DVER=\"$(VER)\" -MMD -MP
LDFLAGS := -lpthread

CFLAGS_DEBUG    := -g3 -O0 -DDEBUG -Wall -Wformat
LDFLAGS_DEBUG   :=
CFLAGS_RELEASE  := -O2 -DNDEBUG
LDFLAGS_RELEASE :=

CFLAGS += $(CFLAGS_DEBUG)

SRCS := $(wildcard *.c)
DEPS := $(SRCS:.c=.c.d)
OBJS := $(SRCS:.c=.c.o)

$(APP): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
%.c.o: %.c
	$(CC) -o $@ -c $^ $(CFLAGS)
#-include $(DEPS)

test:

clean:
	$(RM) $(OBJS) $(DEPS) $(APP)

distclean: clean

pkg:

tar:

.PHONY: test clean distclean pkg tar
