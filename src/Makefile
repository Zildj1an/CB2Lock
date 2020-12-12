CC=gcc
CFLAGS=-lpthread -I. -D_GNU_SOURCE -g #-D__APPLY_MAP_K__

all:
	g++ -c map.cpp -o map.o
	$(CC) -c -O0 test_prios.c cb2_lock.c inherit_lock.c \
		protect_lock.c mutex_lock.c $(CFLAGS)
	g++ *.o -o test_prios $(CFLAGS)
clean:
	rm *.o test_prios &> /dev/null

.PHONY: all clean
