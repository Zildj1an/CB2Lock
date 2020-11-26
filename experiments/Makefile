CC=gcc
CFLAGS=-lpthread -I. -D_GNU_SOURCE

all:
	$(CC) test_prios.c -o test_prios $(CFLAGS)
clean:
	rm *.o test_prios &> /dev/null

.PHONY: all clean
