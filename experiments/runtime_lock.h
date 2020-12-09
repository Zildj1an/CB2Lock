#pragma once

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
			int sum_tickets;
		};
	};
} runtime_lock_attr;


typedef struct _runtime_lock {
	int type;
	char *description;

	void (*lock)(void);
	void (*unlock)(void);

	void (*init)(runtime_lock_attr *attr);
	void (*destroy)(void);
} runtime_lock;

extern struct _runtime_lock mutex_lock;
extern struct _runtime_lock protect_lock;
