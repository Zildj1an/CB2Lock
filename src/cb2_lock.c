/*
  ##################################################################
  #  This is the first version of the CB2Lock.                     #
  #  Authors Christopher Blackburn and Carlos Bilbao.		   #
  #  December 2020.						   #
  ##################################################################
*/
#include "runtime_lock.h"
#include "util.h"

#ifdef __APPLY_MAP_K__
#include "map.h"
#endif

static pthread_mutex_t lock;
static pthread_mutex_t meta_lock;
static __thread int original_priority = 0;

static volatile pid_t owner_tid = 0;
static volatile int owner_priority;

static volatile int bystander_tickets_cpu;

static time_t t;

/* The K factor accounts for the number of times the high-priority thread
 * and the low-priority thread have benefited from the Priority Inversion.
 * We need to maintain a hash map of pids-times and also apply a tunning
 * factor initial_K, set to 10 by default.
*/
int compute_times_factor(pid_t HP_pid)
{
	int ret, initial_K = ret = bystander_tickets_cpu;

	/* TODO For future work we can tune the value of initial_K
	   applying P.I.D for a particular benchmark and metrics
	   (and other generic initial_k for any given benchmark.)
	*/ 

#ifdef __APPLY_MAP_K__
	insert_if_new(HP_pid);
	ret = initial_K;
	ret -= get_and_increase(HP_pid);
	ret = (ret > 0)? ret : 0;

	if (ret != 0) LOG_DEBUG("Ret is %d\n", ret);

#endif
	return ret;
}

/* Lottery system to guarantee fairness on the affected core */
int cb2_lock_inversion(int HP_prio, pid_t HP_pid)
{
	int winning_ticket,sum = bystander_tickets_cpu;
	int tickets_LP, K, ret = 0;

	K = compute_times_factor(HP_pid);

	tickets_LP = HP_prio + owner_priority + K;
	
	sum += tickets_LP;

	winning_ticket = rand() % sum;

	/* Has the high-priority thread won the lottery? */
	if (winning_ticket > bystander_tickets_cpu){
		ret = 1;
	} 
	else {
#ifdef __APPLY_MAP_K__
		map_decrease(HP_pid);
#endif
	}
	
	return ret;
}

static void cb2_lock(void)
{
	pid_t me = gettid();
	int rc;

	original_priority = getpriority(PRIO_PROCESS, me);

	if (original_priority == -1) {
		errExit("Error getting the thread priority");
	}

try_again:
	/* Acquire the metadata lock then the other mutex */
	pthread_mutex_lock(&meta_lock);

	rc = pthread_mutex_trylock(&lock);

	if (rc == 0) {
		/* We acquired the lock. Set metadata and continue into CS */
		owner_tid = me;
		LOG_DEBUG("got it %d\n", me);

		if (sched_getcpu() == 0) {
			if (setpriority(PRIO_PROCESS, me, 19) == -1) {
				errExit("Error setting the thread priority");
			}
		}

		pthread_mutex_unlock(&meta_lock);
	} 
	else if (rc == EBUSY) {
		/* We did not acquire the lock. We might be able to update
		 * owner priority to speed things up. */

		assert(owner_tid != -1);
		owner_priority = getpriority(PRIO_PROCESS, owner_tid);
		if (owner_priority == -1) {
			errExit("Error getting the owner priority");
		}

		/* If the priority of the owner is already high enough, then we can
		 * just sleep on the main lock */
		LOG_DEBUG("owner %d\tme %d\n", owner_priority, original_priority);
		if (owner_priority > original_priority) {
			LOG_DEBUG("time to beef up the owner %d\n", me);

			/* Can we update his priority? */
			if (cb2_lock_inversion(original_priority,me)){
				LOG_DEBUG("HEY, in lock inversion %d\n", me);

				/* Raise owner priority */
				if (setpriority(PRIO_PROCESS, owner_tid, 
				   original_priority) == -1) {
					errExit("Error setting the owner priority");
				}
			}
			pthread_mutex_unlock(&meta_lock);
			goto try_again;
		}

		/* Now, we can wait for the main lock */
		pthread_mutex_unlock(&meta_lock);

		LOG_DEBUG("now we wait... %d\n", me);
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

static void cb2_unlock(void)
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

static void 
cb2_init(runtime_lock_attr *attr)
{
	int rc = 0;
	rc |= pthread_mutex_init(&lock, NULL);
	rc |= pthread_mutex_init(&meta_lock, NULL);

	if (rc != 0) {
		errExit("failed to init CB2lock");
	}

	/* Initialize random num generator */
	srand((unsigned) time(&t));

	/* We should update this value everytime a thread is assigned to a core,
	*  but unless this is implemented at CFS, it's a big overhead. For the 
	*  purpose of this proof of concept, we will assume a scenario where 
	*  bystander threads do not come and go, without losing generality.                                     
	*/   
	bystander_tickets_cpu = attr->by_tickets_cpu;
	assert(bystander_tickets_cpu > 0 && "We need a positive value of tickets");
}

static void cb2_destroy(void) 
{
	int rc = 0;
	rc |= pthread_mutex_destroy(&lock);
	rc |= pthread_mutex_destroy(&meta_lock);

	if (rc != 0) {
		errExit("failed to destroy CB2lock");
	}
}

runtime_lock CB2_lock = {
	.type         = RT_CB2,
	.description  = "Mutex with CB2Lock",
	.lock         = cb2_lock,
	.unlock       = cb2_unlock,
	.init         = cb2_init,
	.destroy      = cb2_destroy
};
