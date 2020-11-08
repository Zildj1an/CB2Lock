/*
	Program to test the resilience of CFS to scenarios with
	low-priority locks acquiring the lock.

	authors: Christian Blackburn, Carlos Bilbao
*/
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <pthread.h> 
   
void *thread_func(void *vargp) {
 
    /* TODO acquire the lock and etc. */
 
    return NULL; 
} 
   
int main(void) 
{ 
    pthread_t thread1; 
     
    pthread_create(&thread1, NULL, thread_func, NULL); 
    
    /* TODO 

	Assign different priorities
	Force low-prio lock acquisition first
	Measure and plot
	Test different values
	etc.
	... 

    */

    pthread_join(thread1, NULL); 

    exit(EXIT_SUCCESS); 
}

