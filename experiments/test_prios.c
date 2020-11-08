/*
###############################################################################
# Program to test the resilience of CFS to scenarios with low-priority        #
# threads acquiring the lock first.					      #
#									      #
# Authors: Christian Blackburn, Carlos Bilbao			              #
###############################################################################
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>

#define NUM_THREADS (10)
#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

pthread_mutex_t lock; 

void *thread_func(void *vargp) 
{
	/* TODO acquire the lock and etc. */
	// pthread_mutex_lock(&lock); 
	// pthread_mutex_unlock(&lock); 
	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t threads[NUM_THREADS];
	pthread_attr_t thread_attr;
	struct sched_param param;
	int i = 0, priority1 = 20;

	if (argc != 1){
		fprintf(stderr,"Incorrect number of arguments.\n");
		exit(EXIT_FAILURE);
	}

	/* Initialize mutex */
	if (pthread_mutex_init(&lock, NULL) != 0) { 
        	errExit("mutex init");  
    	} 

	/* Initialize default attributes for a thread */
	if (pthread_attr_init(&thread_attr) != 0){
		errExit("default thread attributes init");
	} 

	/* Retrieve the sched params from the thread attributes */
	if (pthread_attr_getschedparam(&thread_attr,&param) != 0){
		errExit("getting sched params");
	}

	/* Create the threads and assign their priorities */
	for (; i < NUM_THREADS; ++i){

		/* Set the priority (TODO set different values) */
		param.sched_priority = priority1;

		if (pthread_attr_setschedparam(&thread_attr,&param) != 0){
			errExit("setting sched params");
		}

    		pthread_create(&threads[i],&thread_attr, thread_func, NULL);
	}

    	/* TODO: 
	   - Force low-prio lock acquisition first. 
	   - Force (or not?) same CPU-affinity to threads.
	   - Measure and plot: Test different values (bash script?), etc.
    	*/
   
	for (i = 0; i < NUM_THREADS; ++i) {
    		pthread_join(threads[i], NULL);
	}

	pthread_attr_destroy(&thread_attr);
	pthread_mutex_destroy(&lock); 

    	exit(EXIT_SUCCESS);
}

