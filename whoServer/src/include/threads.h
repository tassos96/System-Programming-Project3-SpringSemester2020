#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include "statsList.h"

volatile sig_atomic_t finishFlag;

pthread_mutex_t bufferMutex;
pthread_mutex_t threadMutex;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;

struct sockaddr_in clientAddr;
struct sockaddr* clientPtr; 
socklen_t clientLen;

//----------    STRUCTS    ----------//
typedef struct BoundedBuffer
{
    int* fds;
    int start;
    int end;
    int count;
    int size;
} BoundedBuffer;

BoundedBuffer boundedBuffer;


typedef struct Threads
{
    pthread_t* threadsArray;
    int numThreads;
    int count;
    int finish;
}Threads;

char** workersPorts; //String array for listening ports of workers
int* workersFDs;
int workersPortsSize; //Size of string array
StatisticsList statsList; //List of the stats from workers that server keeps
char** countries;
int totalCountries;
int to_run;


//----------    FUNCTIONS    ----------//

//BoundedBuffer
void initializeBoundedBuffer(BoundedBuffer* boundedBuffer, int boundedBufferSize);
void placeToBuffer(BoundedBuffer* boundedBuffer, int acceptedFD);
int obtainFromBuffer(BoundedBuffer* boundedBuffer);

//void printBuffer(BoundedBuffer* boundedBuffer);
void addCountriesToArray();



//Threads
Threads* initializeThreads(int numThreads);
//The thread function
void* threadFunction(void*);
void destroyThreads(Threads* threads, int numThreads);

#endif