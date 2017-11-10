CC := gcc
VER := test

CFLAGS  := -g3 -Wall -Wno-unused -O0 -MMD -MP -I../ -DVER=\"$(VER)\" -fsanitize=address
LDFLAGS := -lpthread -fsanitize=address

SRCS := $(filter-out ../src/yhttp.c, $(wildcard ../src/*.c))
OBJS := $(SRCS:.c=.c.o)
DEPS := $(SRCS:.c=.c.d)

TEST_OBJS := test-log test-event_select test-worker \
	test-http test-list

all: $(TEST_OBJS)

define test
$(1): $(1).c.o $(OBJS)
	@$(CC) -o $$@ $$^ $(LDFLAGS)
	@echo "GEN $$@"
endef
$(foreach t,$(TEST_OBJS),$(eval $(call test,$(t))))

%.c.o: %.c
	@$(CC) -o $@ -c $< $(CFLAGS)
	@echo "CC  $@"
-include $(DEPS)

define rm
	@$(RM) $(1)
	@echo "RM $(1)"

endef
clean:
	$(foreach f,$(sort $(OBJS) $(TEST_OBJS)),$(call rm,$(f)))
distclean: clean
	$(foreach f,$(sort $(DEPS) $(wildcard *.c.[do]) core),$(call rm,$(f)))

.PHONY: clean all distclean
