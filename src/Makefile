CC=gcc
CFLAGS=-lpthread -I. -D_GNU_SOURCE

all:
	g++ -c map.cpp -o map.o
	$(CC) -O0 test_prios.c cb2_lock.c inherit_lock.c \
		protect_lock.c mutex_lock.c -o test_prios.o $(CFLAGS)
	g++ map.o testprios.o -o test_prios
clean:
	rm *.o test_prios &> /dev/null

.PHONY: all clean
