#include "runtime_lock.h"
#include "util.h"

#include <pthread.h>

static pthread_mutex_t lock;

static void _lock(void)
{
	pthread_mutex_lock(&lock);
}

static void _unlock(void)
{
	pthread_mutex_unlock(&lock);
}

static void _init(__attribute__((unused)) runtime_lock_attr *attr) {
	pthread_mutex_init(&lock, NULL);
}

static void _destroy(void) {
	pthread_mutex_destroy(&lock);
}

runtime_lock mutex_lock = {
	.type = RT_NONE,
	.description = "Normal pthread mutex",
	.lock = _lock,
	.unlock = _unlock,
	.init = _init,
	.destroy = _destroy
};

/* old mutex init */
//void init_lock(void)
//{
//	pthread_mutexattr_t attr;
//	int p;
//
//	if (pthread_mutexattr_init(&attr) != 0) {
//		errExit("Could not init mutex attr");
//	}
//
//	switch (mutex_proto) {
//	default:
//	case MP_CB2:
//	case MP_NONE:
//		p = PTHREAD_PRIO_NONE;
//		break;
//	case MP_INHERIT:
//		/* Priority Inheritance */
//		p = PTHREAD_PRIO_INHERIT;
//		break;
//	case MP_PROTECT:
//		p = PTHREAD_PRIO_PROTECT;
//		break;
//	}
//
//	if (pthread_mutexattr_setprotocol(&attr, p) != 0) {
//		errExit("Could not init mutex attr");
//	}
//
//	pthread_mutex_init(&lock, &attr);
//
//	if (pthread_mutexattr_destroy(&attr) != 0) {
//		errExit("Could not destroy mutex attr");
//	}
//}
