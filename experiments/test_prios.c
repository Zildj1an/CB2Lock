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
pthread_mutex_t lock;
int lowest_acquired = 0;

void *thread_func(void *vargp) 
{
	struct test_run *tr = (struct test_run*)vargp;
	struct timespec start;
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

	/* Force the first thread (the lowest prio if flags not set) to acquire first */
	if (tr->id != 0 && !lowest_acquired){
		asm("");
		goto try_again;
	}

	/* Measure how long this thread has the lock */
	pthread_mutex_lock(&lock);

	/* ##########################  CRITICAL SECTION ######################### */

	clock_gettime(CLOCK_MONOTONIC, &start);

	/* If we set this for every thread regardless, we don't need an atomic flag */
	lowest_acquired = 1;

	/* Let's make the time gap more obvious */
	for (; s < 100000; s++){
		asm(""); /* Avoids GCC optimizations */
	}

	pthread_mutex_unlock(&lock);
	clock_gettime(CLOCK_MONOTONIC, &tr->tp);

	tr->tp.tv_sec -= start.tv_sec;
	tr->tp.tv_nsec -= start.tv_nsec;

	return (void*)tr;
}

int main(int argc, char *argv[])
{
	int i, opt, cpu = 0, ncpu = get_nprocs(), thread_count = ncpu, flags = 0;
	pthread_t *threads;
	pthread_attr_t thread_attr;
	struct test_run *tr;
	cpu_set_t cpuset;

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

	/* Initialize mutex */
	if (pthread_mutex_init(&lock, NULL) != 0) {
		errExit("Mutex init");
	}

	/* Initialize barrier */
	if (pthread_barrier_init(&barrier, NULL, thread_count) != 0) {
		errExit("Barrier init");
	}

	/* #### Create the threads and assign their priorities. ################# */

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

	/* TODO: 
	   - Force low-prio lock acquisition first. 
	   - High and low prio threads must share core to try to trick CFS.
	   - Measure and plot: Test different values, etc. 
	*/
   
	/* ############ Join and process results ################################ */

	for (i = 0; i < thread_count; ++i) {

		if (pthread_join(threads[i], (void**)&tr) != 0) {
			errExit("Could not join thread");
		}

		printf("Priority of thread %d: %d\t\t time: %d:%d\n", i,
			     tr->priority, tr->tp.tv_sec, tr->tp.tv_nsec);
		free(tr);
	}

	/* Cleanup */
	pthread_mutex_destroy(&lock); 
	pthread_barrier_destroy(&barrier);

	free(threads);
	pthread_attr_destroy(&thread_attr);

	exit(EXIT_SUCCESS);
}
