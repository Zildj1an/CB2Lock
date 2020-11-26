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
#include <time.h>
#include <errno.h>

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct test_run {
	struct timespec tp;
};

pthread_mutex_t lock;

void *thread_func(void *vargp) {
	struct test_run *tr = (struct test_run*)vargp;
	struct timespec start;

	/* Measure how long this thread has the lock */
	pthread_mutex_lock(&lock);
	clock_gettime(CLOCK_MONOTONIC, &start);

	/* CS */

	pthread_mutex_unlock(&lock);
	clock_gettime(CLOCK_MONOTONIC, &tr->tp);

	tr->tp.tv_sec -= start.tv_sec;
	tr->tp.tv_nsec -= start.tv_nsec;

	return (void*)tr;
}

int main(int argc, char *argv[])
{
	int i, opt, cpu, ncpu = get_nprocs(), thread_count = ncpu, flags = 0;
	pthread_t *threads;
	pthread_attr_t thread_attr;
	struct sched_param sparam;
	struct test_run *tr;
	cpu_set_t cpuset;

	while ((opt = getopt(argc, argv, "hn:f")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s [-n nthreads] [-f]\n");
				printf("\n");
				printf("If -f flag is supplied, then all threads will have same priority\n");
				exit(EXIT_SUCCESS);
			case 'n':
				thread_count = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Usage: %s [-n nthreads] [-f]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if (thread_count <= 0) {
		errExit("invalid number of threads");
	}

	threads = calloc(thread_count, sizeof(pthread_t));
	if (threads == NULL) {
		errExit("Could not calloc threads");
	}

	/* Initialize default attributes for a thread */
	if (pthread_attr_init(&thread_attr) != 0){
		errExit("default thread attributes init");
	} 

	if (pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED) != 0) {
		errExit("could not set explicit schedule");
	}

	if (pthread_attr_setschedpolicy(&thread_attr, SCHED_OTHER) != 0) {
		errExit("could not set explicit schedule");
	}

	/* Retrieve the sched params from the thread attributes */
	if (pthread_attr_getschedparam(&thread_attr, &sparam) != 0){
		errExit("getting sched params");
	}

	/* Initialize mutex */
	if (pthread_mutex_init(&lock, NULL) != 0) {
		errExit("mutex init");
	}

	/* Create the threads and assign their priorities. If we are oversubscribed,
	 * priorities and cpu pinning will wrap around to match the niceness and
	 * nproc of the machine. */
	sparam.sched_priority = 0;
	CPU_ZERO(&cpuset);
	cpu = 0;
	for (i = 0; i < thread_count; i++){

		/* Set the priority */
		sparam.sched_priority = 0;

		if (pthread_attr_setschedparam(&thread_attr, &sparam) != 0) {
			errExit("setting sched params");
		}

		/* Set processor affinity */
		CPU_SET(cpu, &cpuset);
		cpu %= ncpu;
		if (pthread_attr_setaffinity_np(&thread_attr, sizeof(cpu_set_t), &cpuset) != 0) {
			errExit("Could not set thread affinity");
		}

		/* Make a results struct for this thread */
		if ((tr = malloc(sizeof(struct test_run))) == NULL) {
			errExit("Could not malloc test results struct for thread");
		}

		if (pthread_create(&threads[i], &thread_attr, thread_func, tr) != 0) {
			errExit("Could not create thread");
		}
	}

    	/* TODO: 
	   - Force low-prio lock acquisition first. 
	   - Force (or not?) same CPU-affinity to threads.
	   - Measure and plot: Test different values (bash script?), etc. */
   
	/* Join and process results */
	for (i = 0; i < thread_count; ++i) {
		if (pthread_join(threads[i], (void**)&tr) != 0) {
			errExit("Could not join thread");
		}

		/* Process this thread's results */
		if (pthread_attr_getschedparam(&thread_attr, &sparam) != 0){
			errExit("getting sched params");
		}

		printf("Priority of thread %d: %d\t\ttime: %d;%d\n", i,
				sparam.sched_priority, tr->tp.tv_sec, tr->tp.tv_nsec);
		free(tr);
	}

	/* Cleanup */
	pthread_mutex_destroy(&lock); 

	free(threads);
	pthread_attr_destroy(&thread_attr);

	exit(EXIT_SUCCESS);
}

