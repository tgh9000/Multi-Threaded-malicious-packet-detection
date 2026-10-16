#include "dispatch.h"

#include <pcap.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "analysis.h"




// predetermines the maximum number of worker threads within the program.
#define workerCount 10
pthread_t workers[workerCount];

pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueNotEmpty;

// Queue node structure
typedef struct packet_node 
{
    struct pcap_pkthdr *header;
    uint8_t *data;
    int verbose;
    struct packet_node *next;
} packet_node;

struct packet_node *queueHead = NULL;
struct packet_node *queueTail = NULL;


static int shutdownFlag = 0;




// enqueue packet onto the queue so that it can be dealt with by a thread

static void enqueue(struct pcap_pkthdr *head, uint8_t *bits, int verbose)
{
    packet_node *node = malloc(sizeof(struct packet_node));
    node->header = head;
    node->data   = bits;
    node->verbose = verbose;
    node->next = NULL;

    pthread_mutex_lock(&queueLock);
    // adds packet data onto the end of the queue
    if (queueTail == NULL) 
	{
        queueHead = queueTail = node;
    } 
	else 
	{
        queueTail->next = node;
        queueTail = node;
    }

    pthread_cond_signal(&queueNotEmpty);
    pthread_mutex_unlock(&queueLock);
}



// removes and returns the current head of the queue as well as the pointer to that packet.

static packet_node *dequeue()
{
    // locks all variables to do with the queue 
    pthread_mutex_lock(&queueLock);
    // if the queue is empty it waits for the queue to be non empty as long as the shutdown flag has not been set to true 
    while (queueHead == NULL && !shutdownFlag)
	{
        pthread_cond_wait(&queueNotEmpty, &queueLock);
    }
    // if the queue is empty and the program has been told to shut down then the function should return null
    if (shutdownFlag && queueHead == NULL) 
	{
        pthread_mutex_unlock(&queueLock);
        return NULL;
    }
    // gets the current head of the queue
    packet_node *n = queueHead;
    // sets the new head of the queue
    queueHead = queueHead->next;
    if (queueHead == NULL) 
	{
		queueTail = NULL;
	}
    // unlocks all queue related variables
    pthread_mutex_unlock(&queueLock);
    //returns the old head of the queue
    return n;
}


// worker function for each of the threads ( the function that each thread runs when currently executing.)

static void *workerFunction(void *arg)
{
    // makes the argument void since we will not be using it
    (void)arg;

    // will allways run until the dequeu function returns null which is when then function should stop running.
    while (1) 
    {
        //constantly tries to get the next packet
        packet_node *job = dequeue();
        // if there are no more jobs then leave the loop
        if (job == NULL)
        {
            break;
        }
        // analyses the contents of the packet of data according to the specification of the project 
        analyse(job->header, job->data, job->verbose);

        // frees up all pointers to the different parts of the packet as well as the packet itsely so that the mempory can be used later.
        free(job->header);
        free(job->data);
        free(job);
    }
    // returns null because the program has now ended.
    return NULL;
}

// prepares packets so that it can be used by the worker threads
void dispatch(struct pcap_pkthdr *header,const unsigned char *packet,int verbose) 
{
    // manually creates a copy of the packet header in memory so that it cannot be overwritten by anything else in memory.
	struct pcap_pkthdr *headerCopy = malloc(sizeof(struct pcap_pkthdr));
    //copies the packet header into that position in memory.
    memcpy(headerCopy, header, sizeof(struct pcap_pkthdr));

    // creates a copy of the packet data
    uint8_t *dataCopy = malloc(header->caplen);
    //copies the data into memory
    memcpy(dataCopy, packet, header->caplen);

    //adds the new packet to the end of the queue
    enqueue(headerCopy, dataCopy, verbose);

}

// code that deals with making all the threads and makes them analyse the packets.
void dispatchInit()
{
    // creates the mutex lock for the queue
    pthread_mutex_init(&queueLock, NULL);
    // creates a new variable which is used to wake up new worker threads when a new packet arrives and make them go to sleep when there are no packets left.
    pthread_cond_init(&queueNotEmpty, NULL);

    // creates 10 woker threads puts their addresses in the array and then as soon as they are initialised makes them run the worker function.
    for (int i = 0; i < workerCount; i++)
    {
        pthread_create(&workers[i], NULL, workerFunction, NULL);
    }
}


// handles the ending of the program
void dispatchShutdown()
{
    // locks the mutex so that the flag can be set 
    pthread_mutex_lock(&queueLock);
    shutdownFlag = 1;
    // wakes up all asleep threads 
    pthread_cond_broadcast(&queueNotEmpty);
    // unlocks the mutex lock so that all threads can see tat the flag has been set and therefore shut down safely
    pthread_mutex_unlock(&queueLock);
    
    // waits for each thread to finish terminating and then cleans up all memory used by the threads.
    for (int i = 0; i < workerCount; i++)
	{
        pthread_join(workers[i], NULL);
	}
}	

