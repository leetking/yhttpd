APP := yhttpd
VER := 0.1.0
PKG := $(APP)-$(VER)

CC := gcc
RM := rm -rf
MKDIR := mkdir -p

CFLAGS  := -DVER=\"$(VER)\" -MMD -MP -D_GNU_SOURCE
LDFLAGS := -lpthread

CFLAGS_DEBUG    := -g3 -O0 -DDEBUG -Wall -Wno-unused -Wno-format-truncation -Wformat \
	-DYHTTP_DEBUG -fsanitize=address
LDFLAGS_DEBUG   := -fsanitize=address -rdynamic
CFLAGS_RELEASE  := -O2 -DNDEBUG -g -Wall -Werror
LDFLAGS_RELEASE :=

CFLAGS  += $(CFLAGS_DEBUG)
LDFLAGS += $(LDFLAGS_DEBUG)

SRCS := $(sort $(wildcard *.c))
DEPS := $(SRCS:.c=.c.d)
OBJS := $(SRCS:.c=.c.o)

$(APP): $(OBJS)
	@$(CC) -o $@ $^ $(LDFLAGS)
	@echo "GEN $@"
%.c.o: %.c
	@$(CC) -o $@ -c $< $(CFLAGS)
	@echo "CC  $@"
-include $(DEPS)

define rm
	@$(RM) $(1)
	@echo "RM $(1)"

endef
clean:
	$(foreach f,$(sort $(OBJS) $(DEPS) $(APP)),$(call rm,$(f)))

distclean: clean

tar:

.PHONY: clean distclean tar
