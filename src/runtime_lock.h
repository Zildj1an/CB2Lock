#pragma once
#ifndef __RUNTIME_H_
#define __RUNTIME_H_

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#define RT_NONE 0
#define RT_INHERIT 1
#define RT_PROTECT 2
#define RT_CB2 3

typedef struct _runtime_lock_attr {
	union {
		/* protect lock */
		int ceiling;

		/* CB2 */
		struct {
			int by_tickets_cpu;
		};
	};
} runtime_lock_attr;

/*
	This is the struct with the functions that any lock we create
	should implement.
*/
typedef struct _runtime_lock {

	int type;
	char *description;

	void (*lock)(void);
	void (*unlock)(void);

	void (*init)(runtime_lock_attr *attr);
	void (*destroy)(void);

} runtime_lock;

extern struct _runtime_lock mutex_lock;
extern struct _runtime_lock inherit_lock;
extern struct _runtime_lock protect_lock;
extern struct _runtime_lock CB2_lock;

#endif
