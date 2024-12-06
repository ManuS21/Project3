#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define NUM_WORKERS 10
#define THRESHOLD 500
#define TOTAL 1000

void* bank(void* arg);

void* worker(void* arg);


pthread_mutex_t worker_lock;
pthread_mutex_t counter_lock;
pthread_cond_t bank_cond;
pthread_cond_t worker_cond;


pthread_t *workers; // workers

pthread_t bank_thread;

int counter = 0;
int total_transactions = 0;

int bank_signaled = 0;      // bool to track if our bank is performing an update


/*
    Think what lock should bank_cond and worker_cond rely on? When and where should we lock and unlock this lock?

*/



int main(){


    pthread_mutex_init(&worker_lock, NULL);
    pthread_mutex_init(&counter_lock, NULL);
    pthread_cond_init(&bank_cond, NULL);
    pthread_cond_init(&worker_cond, NULL);

    workers = (pthread_t *)malloc(sizeof(pthread_t) * NUM_WORKERS);

    int numbers[NUM_WORKERS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};      // give some "ids"!


    pthread_create(&bank_thread, NULL, bank, NULL);

    for (int i = 0; i < NUM_WORKERS; ++i)
    {
        pthread_create(&workers[i], NULL, worker, (void*)&(numbers[i]));        // create our workers
    }

    for (int j = 0; j < NUM_WORKERS; ++j){
            pthread_join(workers[j], NULL);             // wait on all of our workers
    }       

    pthread_join(bank_thread, NULL);            // wait on the bank thread


    printf("The bank is now closed!\n");

    free(workers);
    pthread_mutex_destroy(&worker_lock);
    pthread_mutex_destroy(&counter_lock);

    pthread_cond_destroy(&worker_cond);
    pthread_cond_destroy(&bank_cond);


}


void* worker(void* arg){

    int *id = (int *)arg;

    // doing work, until threshold is reached!

    while (1){


        // do work on our counter
        pthread_mutex_lock(&counter_lock);
        counter++;
        total_transactions++;

        // we've reached a threshold to signal bank (only one thread should be in here at a time)
        if (counter >= THRESHOLD){

            // signal bank thread to wake up, and set signaled bool

            // while its signaled, cond wait on the bank to broadcast
                // note: how do we make sure our other worker threads dont busy wait on a lock when the bank is signaled and doing work?


        }
        
        counter = 0;    // reset counter

        // weve done all our work break out and exit
        if (total_transactions >= TOTAL) {
            pthread_mutex_unlock(&counter_lock);
            break;
        }

        pthread_mutex_unlock(&counter_lock);
    }

    printf("worker %d finished work! :)\n", *id);
    pthread_exit(NULL);

}


void* bank(void* arg){

    // infinite loop until work is done
    while (1){

        // while bank is not signaled, cond wait on signal from worker

        // do our work...
        usleep(100);    // when testing remove this to make sure u dont deadlock



        // set bank_signaled to false and broadcast to workers to resume

        // done with all our work, break out and exit
        if (total_transactions >= TOTAL){
            break;
        }

    }



    printf("The bank exited...\n");
    pthread_exit(NULL);

}















