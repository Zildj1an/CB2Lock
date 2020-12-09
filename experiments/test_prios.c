/*
###############################################################################
# Program to test the resilience of CFS to scenarios with low-priority        #
# threads acquiring the lock first.                                           #
#                                                                             #
# Authors: Christopher Blackburn, Carlos Bilbao                               #
###############################################################################
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>

#include "util.h"
#include "runtime_lock.h"

#define HIGHEST_PRIO  (-20)
#define LOWEST_PRIO    (19)
#define HIGH_PRIO_CPU  (1)
#define LOW_PRIO_CPU   (0)

/* Control for forcing the scenario we want */
pthread_barrier_t barrier;
volatile int lowest_acquired = 0;
volatile int highest_acquired = 0;

runtime_lock *our_lock = NULL;

/* Encapsulates per-thread test data */
struct test_run {
	struct timespec tp;
	/* Priority of the thread that attempts to acquire the lock */
	int priority;
	/* Core to which this threads is assigned to */
	int pinning;
	int id;

	pid_t tid;
};

void timeval_substract(struct timespec *result, 
		  struct timespec *x, 
		  struct timespec *y)
{
	int nsec;
	unsigned long long nano = 1000000000;
  	
	if (x->tv_nsec < y->tv_nsec) {
    		nsec = (y->tv_nsec - x->tv_nsec) / nano + 1;
    		y->tv_nsec -= nano * nsec;
    		y->tv_sec += nsec;
  	}

  	if (x->tv_nsec - y->tv_nsec > nano) {
    		nsec = (x->tv_nsec - y->tv_nsec) / nano;
    		y->tv_nsec += nano * nsec;
    		y->tv_sec -= nsec;
  	}

  	result->tv_sec = x->tv_sec - y->tv_sec;
  	result->tv_nsec = x->tv_nsec - y->tv_nsec;
}

int compute_percentage(struct test_run *tr, long long int total)
{
	long long int part, nano = 1000000000;
	part  = (tr->tp.tv_sec * nano) + tr->tp.tv_nsec;
	return ((double)part / total) * 100;
}

int init_lock(int lock_proto)
{
	runtime_lock_attr attr;

	switch (lock_proto) {
	case RT_INHERIT:
	case RT_NONE:
		our_lock = &mutex_lock;
		break;
	case RT_PROTECT:
		our_lock = &protect_lock;
		attr.ceiling = HIGHEST_PRIO;
		break;
	default:
		/* unknown protocol */
		return -1;
	}

	our_lock->init(&attr);
	return 0;
}

/********************* the real code *******************/

void bystander_stuff(void)
{
	int s;

	/* Loop until the highest priority thread acquires the lock */
	while (!highest_acquired) {
		/* Chill... */
		for (s = 0; s < 100; s++){
			asm(""); /* Avoids GCC optimizations */
		}
	}
}

void *thread_func(void *vargp) 
{
	struct test_run *tr = (struct test_run*)vargp;
	struct timespec start, aux_time;
	int rc, s = 0, m = 0;

	tr->tid = gettid();
	if (setpriority(PRIO_PROCESS, tr->tid, tr->priority) == -1 ){
		errExit("Error setting the thread priority");
	}

	/* Wait for all threads to start */
	rc = pthread_barrier_wait(&barrier);

	if (rc != PTHREAD_BARRIER_SERIAL_THREAD && rc != 0) {
		errExit("pthread barrier error");
	}

	/* If this is a bystander thread... */
	if (tr->id != HIGH_PRIO_CPU && tr->id != LOW_PRIO_CPU) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		bystander_stuff();
		goto out;
	}

try_again:

	/* Force the first thread (the lowest prio if !flags) acquire first  */
	if (tr->id != 0 && !lowest_acquired){
		asm("");
		goto try_again;
	}

	/* Measure how long this thread has the lock */
	our_lock->lock();

	/* Highest priority thread acquired the lock */
	if (tr->id == 1) {
		highest_acquired = 1;
	}

	/* #####################  CRITICAL SECTION ######################### */

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);

	/* If we set this for every thread, we don't need an atomic flag     */
	lowest_acquired = 1;

	/* Let's make the time gap more obvious */
	for (; s < 1000000; s++){
		asm(""); /* Avoids GCC optimizations */
	}

	our_lock->unlock();

out:
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tr->tp);

	timeval_substract(&aux_time,&tr->tp,&start);

	if (aux_time.tv_sec > 0){
		tr->tp.tv_sec  = aux_time.tv_sec;
	}
	else {
		tr->tp.tv_sec = 0;
	}

	tr->tp.tv_nsec = aux_time.tv_nsec;

	return (void*)tr;
}

int main(int argc, char *argv[])
{
	int i, opt, ncpu = get_nprocs(), thread_count = ncpu, flags = 0;
	pthread_t *threads;
	pthread_attr_t thread_attr;
	struct test_run *tr, *collection_tr;
	/* bench_time is for the execution of all threads and total_time
           is the sum of per-thread CPU time.
	*/
	struct timespec start_bench, end_bench, total_time, bench_time;
	cpu_set_t cpuset;
        long long int total, nano = 1000000000;
	time_t t;

	if (ncpu < 2) {
		errExit("This benchmark requires at least 2 cores to run\n");
	}
	
	while ((opt = getopt(argc, argv, "hn:p:")) != -1) {
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
				if (init_lock(atoi(optarg)) < 0) {
					errExit("Not a valid mutex protocol");
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
	if (!our_lock) {
		init_lock(RT_NONE);
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
	if (!(collection_tr = calloc(thread_count, sizeof(struct test_run)))){
		errExit("Could not calloc collection_tr");
	}

	/* #### Create the threads and assign their priorities. ################# */

	clock_gettime(CLOCK_MONOTONIC, &start_bench);

	printf("\nExperiment with %d threads,",thread_count);
	
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
		tr->pinning = HIGH_PRIO_CPU;

		if (i == LOW_PRIO_CPU) {
			tr->priority = LOWEST_PRIO;
			tr->pinning = LOW_PRIO_CPU;
		} 
		else if (i == HIGH_PRIO_CPU) {
			tr->priority = HIGHEST_PRIO;
		} else {
			/* This thread is a bystander, and his location and 
		           priority level will be random, but in between the high
		           and the low priority threads (-20,19)
			*/
			tr->priority = rand() % 37;
			
			/* Get values between -1 and -19 */ 
			if (tr->priority > 18){
				tr->priority = 0 - tr->priority + 18 ;
			}

			tr->pinning = rand() % 1;
		}

		CPU_ZERO(&cpuset);
		CPU_SET(tr->pinning, &cpuset);

		if (pthread_attr_setaffinity_np(&thread_attr,sizeof(cpu_set_t),&cpuset) != 0) {
			errExit("Could not set thread affinity");
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
		
		total_time.tv_nsec  += tr->tp.tv_nsec;	
		total_time.tv_sec  += tr->tp.tv_sec;

		collection_tr[i].tp.tv_sec  = tr->tp.tv_sec;
		collection_tr[i].tp.tv_nsec = tr->tp.tv_nsec;
		collection_tr[i].priority   = tr->priority;
		collection_tr[i].pinning    = tr->pinning;
	}

	total = total_time.tv_sec * nano + total_time.tv_nsec; 
	
	for (i = 0; i < thread_count; ++i){

		tr = &collection_tr[i];

		printf("Thread: %d\tPriority: %d\tCPU affinity: %d\tCPU time: %d:%09d\tCPU%: %d\n",
				i, tr->priority, tr->pinning, tr->tp.tv_sec, tr->tp.tv_nsec,
				compute_percentage(tr,total));
	
	}

	/* Compute total execution time */
	clock_gettime(CLOCK_MONOTONIC, &end_bench);
	
	timeval_substract(&bench_time,&end_bench,&start_bench);

	printf("Total clock time: %d:%09d\n",
		(long long)bench_time.tv_sec,bench_time.tv_nsec);

	printf("Total threads CPU time: %d:%09d\n",
		(long long)total_time.tv_sec,total_time.tv_nsec);
	
	/* Cleanup */
	our_lock->destroy();
	pthread_barrier_destroy(&barrier);
	free(threads);
	free(collection_tr);
	pthread_attr_destroy(&thread_attr);

	exit(EXIT_SUCCESS);
}
