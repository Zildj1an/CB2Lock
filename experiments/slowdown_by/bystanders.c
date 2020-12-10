/* #############################################################################
   Run the Bystander alone to get his Slowdown 
   Slowdown = Execution time alone / Execution time with the rest
   #############################################################################
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
#include <pthread.h>

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

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

int highest_acquired = 1;

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

void bystander_stuff(void)
{
	int s;

	/* Loop until the highest priority thread acquires the lock */
	while (!highest_acquired) {
		/* Chill... */
		for (s = 0; s < 1000000000; s++){
			asm(""); /* Avoids GCC optimizations */
		}
	}
}

void *thread_func(void *vargp) 
{
	struct test_run *tr = (struct test_run*)vargp;
	struct timespec start, aux_time;
	int rc, s = 0, m = 0;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	bystander_stuff();
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
	int i, opt, ncpu = get_nprocs(), thread_count = 1,flags = 0;
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

	/* Allocate memory for the array of 1 threads */
	if (!(threads = calloc(thread_count, sizeof(pthread_t)))){
		errExit("Could not calloc threads");
	}

	/* Initialize default attributes for a thread */
	if (pthread_attr_init(&thread_attr) != 0){
		errExit("Default thread attributes init");
	} 

	/* Set the CFS scheduler (Most likely it already was) */
	if (pthread_attr_setschedpolicy(&thread_attr, SCHED_NORMAL) != 0) {
		errExit("Could not set the CFS scheduler");
	}

	/* Allocate memory for the array of 1 test run */
	if (!(collection_tr = calloc(thread_count, sizeof(struct test_run)))){
		errExit("Could not calloc collection_tr");
	}

	/* #### Create the thread  ############## */

	clock_gettime(CLOCK_MONOTONIC, &start_bench);

	for (i = 0; i < thread_count; i++){
		/* Make a results struct for this thread */
		if (!(tr = malloc(sizeof(struct test_run)))) {
			errExit("Could not malloc test results struct for thread");
		}

		/* Set priorities and CPU affinity based on thread number */
		tr->id = i;
		tr->pinning = 0;
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

		printf("%d\n",tr->tp.tv_nsec);
	}

	/* Cleanup */
	free(threads);
	free(collection_tr);
	pthread_attr_destroy(&thread_attr);

	exit(EXIT_SUCCESS);
}
