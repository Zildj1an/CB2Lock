#include "runtime_lock.h"
#include "util.h"

static pthread_mutex_t lock;
static volatile int ceiling = 0;
static __thread int original_priority = 0;

static void _lock(void)
{
	pid_t me = gettid();

	original_priority = getpriority(PRIO_PROCESS, me);

	if (original_priority == -1) {
		errExit("Error getting the thread priority");
	}

	/* Raise priority to ceiling */
	if (setpriority(PRIO_PROCESS, me, ceiling) == -1) {
		errExit("Error setting the thread priority");
	}

	pthread_mutex_lock(&lock);
}

static void _unlock(void)
{
	pid_t me = gettid();

	pthread_mutex_unlock(&lock);

	/* Return to original priority */
	if (setpriority(PRIO_PROCESS, me, original_priority) == -1) {
		errExit("Error setting the thread priority");
	}
}

static void _init(runtime_lock_attr *attr)
 {
	pthread_mutex_init(&lock, NULL);

	if (!attr) {
		errExit("protect lock needs attr");
	}

	ceiling = attr->ceiling;
}

static void _destroy(void) {
	pthread_mutex_destroy(&lock);
}

runtime_lock protect_lock = {
	.type         = RT_PROTECT,
	.description  = "Mutex with ceiling PI",
	.lock         = _lock,
	.unlock       = _unlock,
	.init         = _init,
	.destroy      = _destroy
};
