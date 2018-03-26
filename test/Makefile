CC := gcc
VER := test

SANITIZE :=
SANITIZE := -fsanitize=address

CFLAGS  := -g -Wall -Wno-format-truncation -Wno-unused -O0 -MMD -MP -DVER=\"$(VER)\" \
	-DYHTTP_DEBUG $(SANITIZE)
LDFLAGS := -lpthread $(SANITIZE)

SRCS := $(filter-out ../src/yhttp.c, $(wildcard ../src/*.c))
OBJS := $(SRCS:.c=.c.o)
DEPS := $(SRCS:.c=.c.d)

TEST_OBJS := test-log test-event_select test-worker \
	test-http_parser test-list test-http_error_page
TEST_ALONE := test-bench

all: $(TEST_OBJS)

define test
$(1): $(1).c.o $(OBJS)
	@$(CC) -o $$@ $$^ $(LDFLAGS)
	@echo "GEN $$@"
endef
$(foreach t,$(TEST_OBJS),$(eval $(call test,$(t))))

test-bench: test-bench.c.o
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
	$(foreach f,$(sort $(OBJS) $(TEST_OBJS) $(TEST_ALONE)),$(call rm,$(f)))
distclean: clean
	$(foreach f,$(sort $(DEPS) $(wildcard *.c.[do] *.d) core),$(call rm,$(f)))

.PHONY: clean all distclean
