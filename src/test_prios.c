/*
###############################################################################
# Microbenchmark to test the fairness of Priority Inversion on scenarios with #
# low-priority threads acquiring the lock first, between current proposals    #
# and our mutex CB2Lock.                                                      #
#                                                                             #
# Authors: Christopher Blackburn, Carlos Bilbao (2020)                        #
###############################################################################
*/
#include "util.h"
#include "runtime_lock.h"

#define HIGHEST_PRIO  (-20)
#define LOWEST_PRIO    (19)
#define HIGH_PRIO_CPU  (1)
#define LOW_PRIO_CPU   (0)

#define BILLION 1000000000

/* Control for forcing the scenario we want */
pthread_barrier_t barrier;
static volatile int lowest_acquired = 0;
static volatile int highest_acquired = 0;
static volatile int done = 0;

/* Our lock, that will be of the type specified at runtime */
runtime_lock *our_lock = NULL;

/* Encapsulates per-thread test data */
struct test_run {
	struct timespec tp;
	/* Priority of the thread that attempts to acquire the lock */
	int priority;
	/* Core to which this threads is assigned to */
	int pinning;
	int id;

	/* How many times to acquire the lock */
	int iter;

	pid_t tid;
};

void timeval_substract(struct timespec *result, struct timespec *new,
		struct timespec *old)
{
	if (new->tv_nsec < old->tv_nsec) {
		result->tv_sec = new->tv_sec - 1 - old->tv_sec;
		result->tv_nsec = new->tv_nsec - old->tv_nsec + BILLION;
	} 
	else {
		result->tv_sec = new->tv_sec - old->tv_sec;
		result->tv_nsec = new->tv_nsec - old->tv_nsec;
	}
}

void timeval_accumulate(struct timespec *total, struct timespec *toadd)
{
	total->tv_sec += toadd->tv_sec;
	total->tv_nsec += toadd->tv_nsec;

	if (total->tv_nsec >= BILLION) {
		total->tv_nsec -= BILLION;
		total->tv_sec++;
	}
}

int compute_percentage(struct test_run *tr, long long int total)
{
	long long int part;
	part = (tr->tp.tv_sec * BILLION) + tr->tp.tv_nsec;
	return ((double)part / total) * 100;
}

void __security_check(void)
{
	assert(our_lock->lock && "You need to implement a lock function buddy");
	assert(our_lock->unlock && "I don't see any unlock in your lock...");
	assert(our_lock->init && "We need a init function in your lock");
	assert(our_lock->destroy && "Where is the destroy() for the lock?");
}

int init_lock(int lock_proto, int sum_bys)
{
	runtime_lock_attr attr;

	switch (lock_proto) {
	case RT_NONE:
		our_lock = &mutex_lock;
		break;
	case RT_INHERIT:
		our_lock = &inherit_lock;
		break;
	case RT_PROTECT:
		our_lock = &protect_lock;
		attr.ceiling = HIGHEST_PRIO;
		break;
	case RT_CB2:
		our_lock = &CB2_lock;
		attr.by_tickets_cpu = sum_bys;
		break;
	default:
		/* unknown protocol */
		return -1;
	}

	/* Make sure we don't step into null pointers in the future ... */
	__security_check();

	our_lock->init(&attr);
	
	return 0;
}

/********************* the real code *******************/

void bystander_stuff(struct test_run *tr, struct timespec *aux_time,
		struct timespec *start, struct timespec *end)
{
	int s;
	tr->iter = 0;

	while (1) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, start);
		/* Chill... */
		for (s = 0; s < 1000; s++) {
			asm("");
		}

		/* if the lowest has the lock, measure time and iterations. 
		 * Otherwise, we stop counting */
		if (lowest_acquired) {
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, end);
			tr->iter++;

			timeval_substract(aux_time, end, start);
			timeval_accumulate(&tr->tp, aux_time);
		}

		if (done) {
			break;
		}
	}
}

void *thread_func(void *vargp) 
{
	struct test_run *tr = (struct test_run*)vargp;
	struct timespec start, end, aux_time;
	int rc, s, m = 0, i;

	/* Sanity init */
	tr->tp.tv_sec = 0;
	tr->tp.tv_nsec = 0;
	tr->tid = gettid();

	if (setpriority(PRIO_PROCESS, tr->tid, tr->priority) == -1 ){
		errExit("Error setting the thread priority");
	}

	/* Wait for all threads before beginning next iteration */
	rc = pthread_barrier_wait(&barrier);
	
	if (rc != PTHREAD_BARRIER_SERIAL_THREAD && rc != 0) {
		errExit("pthread barrier error");
	}

	/* If this is a bystander thread... */
	if (tr->id != HIGH_PRIO_CPU && tr->id != LOW_PRIO_CPU) {
		LOG_DEBUG("Hi it's thread %d\n",tr->id);
		bystander_stuff(tr, &aux_time, &start, &end);
		goto out;
	}

	LOG_DEBUG("Hi it's thread %d\n",tr->id);

	for (i = 0; i < tr->iter; i++) {

		/* Measure how long this thread has the lock */
		LOG_DEBUG("Trying to get lock, I am %d\n", tr->id);
		our_lock->lock();

		LOG_DEBUG("I (%d) have acquired the lock\n", tr->id);

		if (tr->id == 0 && done) {
			our_lock->unlock();
			break;
		}

		if (tr->id == LOW_PRIO_CPU) {
			lowest_acquired = 1;
		} else {
			highest_acquired = 1;
		}
		
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);

		/* #####################  CRITICAL SECTION ################# */

		/* Let's make the time gap more obvious */
		for (s = 0; s < BILLION; s++){
			asm(""); /* Avoids GCC optimizations */
		}

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		
		if (tr->id == LOW_PRIO_CPU) {
			if (setpriority(PRIO_PROCESS, tr->tid, HIGHEST_PRIO) == -1) {
				errExit("Error setting the thread priority");
			}
		}

		if (i + 1 == tr->iter) {
			done = 1;
		}

		our_lock->unlock();

		/* Enforce ordering */
		if (tr->id) {
			lowest_acquired = 0;
		} else {
			highest_acquired = 0;
		}

		LOG_DEBUG("I (%d) have released the lock\n", tr->id);

		timeval_substract(&aux_time, &end, &start);

		/* Compute time spent in CS, add to total time for this thread */
		timeval_accumulate(&tr->tp, &aux_time);
	}

out:
	return (void*)tr;
}

void clean_l1_l2(void);

int main(int argc, char *argv[])
{
	/* Create a new mapping in the virtual address space for this process */
	void *addr = mmap(NULL,sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE, 
		MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	int opt, ncpu = get_nprocs(), thread_count = 3, flags = 0, is_cb2 = 0,
		iter = 1;
	pthread_t *threads;
	pthread_attr_t thread_attr;
	struct test_run *tr, **collection_tr;
	/* bench_time is for the execution of all threads and total_time is the sum
	 * of per-thread CPU time. */
	struct timespec start_bench, end_bench, total_time, bench_time;
	cpu_set_t cpuset;
	long long int total;
	time_t t;
	register int i;
	int sum_bys = 0;

	if (ncpu < 2) {
		errExit("This benchmark requires at least 2 cores to run\n");
	}
	
	while ((opt = getopt(argc, argv, "hn:p:i:")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s [-n nthreads]\n",argv[0]);
				printf("\n");
				printf("If -f flag is supplied, then all threads will have same priority\n");
				exit(EXIT_SUCCESS);
			case 'n':
				thread_count = atoi(optarg);
				if (thread_count < 3){
					fprintf(stderr, "This benchmark requires at least 3 threads (%d)!\n",thread_count);
					exit(EXIT_FAILURE);
				}
				break;
			case 'p':
				if (atoi(optarg) != RT_CB2){ 
					if (init_lock(atoi(optarg),0) < 0) {
					    errExit("Not a valid mutex protocol");
					}
				}
				else {
					is_cb2 = 1;
				}
				break;
			case 'i':
				iter = atoi(optarg);
				if (iter < 1){
					errExit("You need at least one iteration...");
				}
				break;
			default:
				fprintf(stderr, "Usage: %s [-n nthreads]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	/* We will need to be root for nice values lower than 0 */
	if (geteuid() != 0){
  		fprintf(stderr,"We need root to change priorities!\n");
		exit(EXIT_FAILURE);
	}

	/* Allocate memory for the array of threads */
	if (!(threads = calloc(thread_count, sizeof(pthread_t)))){
		errExit("Could not calloc threads");
	}

	/* Initialize default attributes for a thread */
	if (pthread_attr_init(&thread_attr) != 0){
		errExit("Default thread attributes init");
	} 

	if (pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED) != 0) {
		errExit("Could not set explicit schedule");
	}

	/* Init lock */
	if (!is_cb2 && !our_lock) {
		init_lock(RT_NONE,0);
	}

	/* Set the CFS scheduler (Most likely it already was) */
	if (pthread_attr_setschedpolicy(&thread_attr, SCHED_NORMAL) != 0) {
		errExit("Could not set the CFS scheduler");
	}

	/* Initialize barrier */
	if (pthread_barrier_init(&barrier, NULL, thread_count) != 0) {
		errExit("Barrier init");
	}

	/* Allocate memory for the array of test run */
	if (!(collection_tr = calloc(thread_count, sizeof(struct test_run*)))){
		errExit("Could not calloc collection_tr");
	}

	/* #### Create the threads and assign their priorities. ################# */

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_bench);

	printf("\nExperiment with lock %s\n%d threads and %d iterations,", 
		(is_cb2) ? "CB2Lock" : our_lock->description, thread_count, iter);
	
	if (flags){
		printf(" all threads with same priority.\n");
	}
	else {
		printf(" thread zero has the lowest priority.\n");
	}

	/* Intialize random number generator */
   	srand((unsigned) time(&t));

	for (i = 0; i < thread_count; i++){
		/* Make a results struct for this thread */
		if (!(tr = malloc(sizeof(struct test_run)))) {
			errExit("Could not malloc test results struct for thread");
		}

		/* Set priorities and CPU affinity based on thread number */
		tr->id = i;
		tr->pinning = LOW_PRIO_CPU;
		tr->iter = iter;

		if (i == LOW_PRIO_CPU) {
			/* In order to allow the test to make progress accross iterations,
			 * we set the low priority thread to highest priority until it grabs
			 * the lock and allows the actual high priority thread to continue.
			 * */
			tr->priority = HIGHEST_PRIO;
		} 
		else if (i == HIGH_PRIO_CPU) {
			tr->priority = HIGHEST_PRIO;
			tr->pinning = HIGH_PRIO_CPU;
		} 
		else {
			/* This thread is a bystander, and his location and priority level
			 * will be random, but in between the high and the low priority
			 * threads (-20,19) */
			tr->priority = rand() % 37;
			
			/* Get values between -1 and -19 */ 
			if (tr->priority > 18){
			        sum_bys += tr->priority;
				tr->priority = 0 - tr->priority + 18 ;
			}
			else if (is_cb2){
				sum_bys += tr->priority;
			}

			tr->pinning = rand() % 1;
		}

		CPU_ZERO(&cpuset);
		CPU_SET(tr->pinning, &cpuset);

		if (pthread_attr_setaffinity_np(&thread_attr,sizeof(cpu_set_t),&cpuset) != 0) {
			errExit("Could not set thread affinity");
		}

		/* init the lock before the threads go off and party */
		if (is_cb2 && i == thread_count - 1) {
			init_lock(RT_CB2, sum_bys);
		}

		if (pthread_create(&threads[i], &thread_attr, thread_func, tr) != 0) {
			errExit("Could not create thread");
		}
	}

	/* ############ Join and process results ########################### */

	total_time.tv_sec  = 0;
	total_time.tv_nsec = 0;

	for (i = 0; i < thread_count; ++i) {

		if (pthread_join(threads[i], (void**)&tr) != 0) {
			errExit("Could not join thread");
		}

		timeval_accumulate(&total_time, &tr->tp);

		collection_tr[i] = tr;
	}

	total = total_time.tv_sec * BILLION + total_time.tv_nsec; 
	
	for (i = 0; i < thread_count; ++i){

		tr = collection_tr[i];

		printf("Thread: %d\tPrio: %3d\tCPU#: %d\tCPU time: %ld:%09ld\tCPU%: %3d\tIters: %d\n",
				i, (i == 0) ? LOWEST_PRIO : tr->priority, tr->pinning,
				tr->tp.tv_sec, tr->tp.tv_nsec, compute_percentage(tr,total),
				tr->iter);
	
		free(tr);
		collection_tr[i] = NULL;
	}

	/* Compute total execution time */
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_bench);
	
	timeval_substract(&bench_time, &end_bench, &start_bench);

	printf("Total benchmark time: %lld:%09ld\n",
		(long long)bench_time.tv_sec,bench_time.tv_nsec);

	printf("Total threads CPU time: %lld:%09ld\n",
		(long long)total_time.tv_sec,total_time.tv_nsec);
	
	/* Cleanup */
	our_lock->destroy();
	pthread_barrier_destroy(&barrier);
	free(threads);
	pthread_attr_destroy(&thread_attr);
	free(collection_tr);

	/* Some stuff to make the experiments more reliable */

	//clean_l1_l2();

	if (madvise(addr,sysconf(_SC_PAGE_SIZE),MADV_DONTNEED)){
		errExit("we couldn't do madvise");	
	}

	exit(EXIT_SUCCESS);
}

void clean_l1_l2(void)
{
	const int size = 40*1024*1024; /*  40 MB!! */
	char *c = (char *)malloc(size);

	for (int i = 0; i < 0xffff; i++){
		for (int j = 0; j < size; j++){
         		c[j] = i*j;	
		}
	}

	free(c);
}
