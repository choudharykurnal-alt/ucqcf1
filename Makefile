# Makefile for the UCQCF Scheduler

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.
LDFLAGS =

# Source files
SCHEDULER_SRCS = scheduler/scheduler.c scheduler/scheduler_state.c scheduler/scheduler_rules.c scheduler/core_affinity.c scheduler/preemption.c
SCHEDULER_OBJS = $(SCHEDULER_SRCS:.c=.o)

TEST_SCHEDULER_SRCS = tests/scheduler/test_scheduler.c
TEST_SCHEDULER_OBJS = $(TEST_SCHEDULER_SRCS:.c=.o)

# Default target
all: scheduler.a

# Build scheduler library
scheduler.a: $(SCHEDULER_OBJS)
	ar rcs $@ $^

# Rule to compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Scheduler tests
scheduler_tests: scheduler_test_runner
	./scheduler_test_runner

scheduler_test_runner: $(SCHEDULER_OBJS) $(TEST_SCHEDULER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# All tests
all_tests: scheduler_tests

# Linting
lint:
	cpplint --filter=-build/header_guard scheduler/*.h scheduler/*.c tests/scheduler/*.c

# Clean
clean:
	rm -f scheduler/*.o tests/scheduler/*.o scheduler.a scheduler_test_runner

.PHONY: all scheduler_tests all_tests lint clean
