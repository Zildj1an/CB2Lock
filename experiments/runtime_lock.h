#pragma once

#define RT_MUTEX 0
#define RT_CB2 1

typedef struct _runtime_lock {
	int type;
	char *description;

	void (*lock)(void);
	void (*unlock)(void);

	void (*init)(void);
	void (*destroy)(void);
} runtime_lock;

extern struct _runtime_lock mutex_lock;
