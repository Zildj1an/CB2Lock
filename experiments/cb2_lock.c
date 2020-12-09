#include "runtime_lock.h"
#include "util.h"

#include <pthread.h>

pthread_mutex_t lock;

static void _cb2_lock(void)
{
	pthread_mutex_lock(&lock);
}

static void _cb2_unlock(void)
{
	pthread_mutex_unlock(&lock);
}

static void _cb2_init(void) {
	pthread_mutex_init(&lock, NULL);
}

static void _cb2_destroy(void) {
	pthread_mutex_destroy(&lock);
}

runtime_lock cb2_lock = {
	.type = RT_CB2,
	.description = "our lock",
	.lock = _cb2_lock,
	.unlock = _cb2_unlock,
	.init = _cb2_init,
	.destroy = _cb2_destroy
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
