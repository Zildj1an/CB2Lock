#include "runtime_lock.h"
#include "util.h"

#include <pthread.h>

static pthread_mutex_t lock;

static void _lock(void)
{
	pthread_mutex_lock(&lock);

	if (sched_getcpu() == 0) {
		if (setpriority(PRIO_PROCESS, gettid(), 19) == -1) {
			errExit("Error setting the thread priority");
		}
	}
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
	.type        = RT_NONE,
	.description = "normal pthread mutex",
	.lock        = _lock,
	.unlock      = _unlock,
	.init        = _init,
	.destroy     = _destroy
};
