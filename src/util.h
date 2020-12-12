#ifndef __UTIL_H_
#define __UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <sys/mman.h>

#define gettid() syscall(SYS_gettid)

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

#ifdef DEBUG
#define LOG_DEBUG(msg, ...) printf("DEBUG: " msg, __VA_ARGS__)
#else
#define LOG_DEBUG(msg, ...) do {} while(0)
#endif

#endif
