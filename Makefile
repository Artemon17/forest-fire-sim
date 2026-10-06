CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 -g -Iinclude -D_POSIX_C_SOURCE=200809L
SRCS    = $(wildcard src/*.c)
OBJS    = $(SRCS:.c=.o)
TARGET  = fire

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
