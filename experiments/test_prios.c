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
#include <time.h>
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>

#define HIGHEST_PRIO (-20)
#define LOWEST_PRIO   (19)

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct test_run {
	struct timespec tp;
	/* Priority of the thread that attempts to acquire the lock */
	int priority;
	int id;
};

pthread_barrier_t barrier;
int lowest_acquired = 0;

pthread_spinlock_t spinlock;

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

void *thread_func(void *vargp) 
{
	struct test_run *tr = (struct test_run*)vargp;
	struct timespec start, aux_time;
	int rc, s = 0, m = 0;

	if (setpriority(PRIO_PROCESS,getpid(),tr->priority) == -1 ){
		errExit("Error setting the thread priority");
	}

	/* Wait for all threads to start */
	rc = pthread_barrier_wait(&barrier);

	if (rc != PTHREAD_BARRIER_SERIAL_THREAD && rc != 0) {
		errExit("pthread barrier error");
	}

try_again:

	/* Force the first thread (the lowest prio if !flags) acquire first  */
	if (tr->id != 0 && !lowest_acquired){
		asm("");
		goto try_again;
	}

	/* Measure how long this thread has the lock */
	 
	pthread_spin_lock(&spinlock);

	/* #####################  CRITICAL SECTION ######################### */

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);

	/* If we set this for every thread, we don't need an atomic flag     */
	lowest_acquired = 1;

	/* Let's make the time gap more obvious */
	for (; s < 1000000; s++){
		asm(""); /* Avoids GCC optimizations */
	}

	pthread_spin_unlock(&spinlock);
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

int compute_percentage(struct test_run *tr, long long int total)
{
	long long int part, nano = 1000000000; 
	part  = (tr->tp.tv_sec * nano) + tr->tp.tv_nsec;
	return ((double)part / total) * 100;
}

int main(int argc, char *argv[])
{
	int i, opt, cpu = 0, ncpu = get_nprocs(), thread_count = ncpu, flags = 0;
	pthread_t *threads;
	pthread_attr_t thread_attr;
	struct test_run *tr, *collection_tr;
	/* bench_time is for the execution of all threads and total_time
           is the sum of per-thread CPU time.
	*/
	struct timespec start_bench, end_bench, total_time, bench_time;
	cpu_set_t cpuset;
        long long int total, nano = 1000000000;
	char *sp1= "  ", sp2 = ' ';
	
	while ((opt = getopt(argc, argv, "hn:f")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s [-n nthreads]\n",argv[0]);
				printf("\n");
				printf("If -f flag is supplied, then all threads will have same priority\n");
				exit(EXIT_SUCCESS);
			case 'n':
				thread_count = atoi(optarg);
				if (thread_count <= 0){
					fprintf(stderr, "Invalid number of threads!\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'f':
				flags = 1;
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

	/* Set the CFS scheduler (Most likely it already was) */
	if (pthread_attr_setschedpolicy(&thread_attr, SCHED_NORMAL) != 0) {
		errExit("Could not set the CFS scheduler");
	}

	/* Initialize spinlock */
 	pthread_spin_init(&spinlock, 0);

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

	for (i = 0; i < thread_count; i++){

		/* Set processor affinity */
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		//cpu = (cpu + 1) % ncpu;

		if (pthread_attr_setaffinity_np(&thread_attr,sizeof(cpu_set_t),&cpuset) != 0) {
			errExit("Could not set thread affinity");
		}

		/* Make a results struct for this thread */
		if (!(tr = malloc(sizeof(struct test_run)))) {
			errExit("Could not malloc test results struct for thread");
		}

		tr->priority = LOWEST_PRIO;
		tr->id = i;

		/* All threads are the highest priority but one (If we didn't do -f) */
		if (!flags && i > 0){
			tr->priority = HIGHEST_PRIO;
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
	}

	total = total_time.tv_sec * nano + total_time.tv_nsec; 
	
	for (i = 0; i < thread_count; ++i){

		tr = &collection_tr[i];

		printf("Thread %d: Priority %s%d\t CPU time: %d:%09d\t CPU%: %d\n",
		       i,(tr->priority > 0)?sp1:&sp2,
		       tr->priority, tr->tp.tv_sec, 
		       tr->tp.tv_nsec, compute_percentage(tr,total));
	}

	/* Compute total execution time */
	clock_gettime(CLOCK_MONOTONIC, &end_bench);
	
	timeval_substract(&bench_time,&end_bench,&start_bench);

	printf("Total clock time: %d:%09d\n",
		(long long)bench_time.tv_sec,bench_time.tv_nsec);

	printf("Total threads CPU time: %d:%09d\n",
		(long long)total_time.tv_sec,total_time.tv_nsec);
	
	/* Cleanup */
	pthread_spin_destroy(&spinlock);
	pthread_barrier_destroy(&barrier);
	free(threads);
	free(collection_tr);
	pthread_attr_destroy(&thread_attr);

	exit(EXIT_SUCCESS);
}
