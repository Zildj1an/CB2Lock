CC=gcc
CFLAGS=-lpthread -I. -D_GNU_SOURCE

all:
	$(CC) -O0 test_prios.c mutex_lock.c -o test_prios $(CFLAGS)
clean:
	rm test_prios &> /dev/null

.PHONY: all clean
