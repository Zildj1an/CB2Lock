#include "runtime_lock.h"
#include "util.h"

/* To implement priority inheritance, we used two locks. One represents the lock
 * for the critical section, while the other locks metadata for setting and
 * updating priorities. Linux implements this a bit differently, but we used
 * this approach for simplicity's sake (see pi-futex.txt in Linux kernel
 * Documentation for more information on Priority Inversion techniques in the
 * kernel). Note that in both implementations, the coordination required to
 * safely handle getting/setting priority levels of threads makes the unlock
 * method no longer wait-free. */

static pthread_mutex_t lock;
static pthread_mutex_t meta_lock;
static __thread int original_priority = 0;

static volatile pid_t owner_tid = 0;

static void _lock(void)
{
	pid_t me = gettid();
	int rc, owner_priority;

	original_priority = getpriority(PRIO_PROCESS, me);

	if (original_priority == -1) {
		errExit("Error getting the thread priority");
	}

	/* Acquire the metadata lock then the other mutex */
	pthread_mutex_lock(&meta_lock);

	rc = pthread_mutex_trylock(&lock);
	if (rc == 0) {
		/* We acquired the lock. Set metadata and continue into CS */
		owner_tid = me;

		if (sched_getcpu() == 0) {
			if (setpriority(PRIO_PROCESS, me, 19) == -1) {
				errExit("Error setting the thread priority");
			}
		}

		pthread_mutex_unlock(&meta_lock);
	} 
	else if (rc == EBUSY) {
		/* We did not acquire the lock. Update owner priority to speed things up
		 * a bit. */
		assert(owner_tid != -1);
		owner_priority = getpriority(PRIO_PROCESS, owner_tid);
	
		if (owner_priority == -1) {
			errExit("Error getting the owner priority");
		}

		/* If the priority of the owner is already high enough, then we can
		 * just sleep on the main lock */
		if (owner_priority > original_priority) {
			/* Raise owner priority */
			if (setpriority(PRIO_PROCESS, owner_tid, original_priority) == -1) {
				errExit("Error setting the owner priority");
			}
		}

		/* Now, we can wait for the main lock */
		pthread_mutex_unlock(&meta_lock);
		pthread_mutex_lock(&lock);

		/* Reacquire the metadata lock, fix metadata, then enter CS */
		pthread_mutex_lock(&meta_lock);
		owner_tid = me;

		if (sched_getcpu() == 0) {
			if (setpriority(PRIO_PROCESS, me, 19) == -1) {
				errExit("Error setting the thread priority");
			}
		}
		pthread_mutex_unlock(&meta_lock);
	} 
	else {
		errExit("something went terribly wrong when we tried to get a lock...");
	}
}

static void _unlock(void)
{
	pid_t me = gettid();

	pthread_mutex_lock(&meta_lock);

	/* Release the CS lock now */
	pthread_mutex_unlock(&lock);

	/* reset priority and metadata */
	owner_tid = -1;
	pthread_mutex_unlock(&meta_lock);

	if (setpriority(PRIO_PROCESS, me, original_priority) == -1) {
		errExit("Error setting the thread priority");
	}
}

static void _init(__attribute__((unused)) runtime_lock_attr *attr)
{
	int rc = 0;
	rc |= pthread_mutex_init(&lock, NULL);
	rc |= pthread_mutex_init(&meta_lock, NULL);

	if (rc != 0) {
		errExit("failed to init inherit lock");
	}
}

static void _destroy(void) 
{
	int rc = 0;
	rc |= pthread_mutex_destroy(&lock);
	rc |= pthread_mutex_destroy(&meta_lock);

	if (rc != 0) {
		errExit("failed to destroy inherit lock");
	}
}

runtime_lock inherit_lock = {
	.type         = RT_INHERIT,
	.description  = "inherit PI",
	.lock         = _lock,
	.unlock       = _unlock,
	.init         = _init,
	.destroy      = _destroy
};
