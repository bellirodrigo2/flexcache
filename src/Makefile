CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -I..

# Source files
TEST_SRCS = tests/test_main.c tests/test_bcache.c
LIB_SRCS = flexcache.c

# Object files
TEST_OBJS = $(TEST_SRCS:.c=.o)

# Output
TARGET = run_tests

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(LIB_SRCS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TEST_OBJS) $(TARGET)

# Dependencies
test_bcache.o: tests/test_bcache.c tests/test_helpers.h bcache.h
test_main.o: tests/test_main.c