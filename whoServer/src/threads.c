#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <netdb.h>	         /* gethostbyaddr */
#include <arpa/inet.h>
#include <signal.h>

#include "include/threads.h"
#include "include/whoServer.h"
#include "include/interface.h"

#define FINISH_TRUE 1
#define FINISH_FALSE 0
#define EXIT 7

//Flag for signal handling
volatile sig_atomic_t finishFlag = 0;

//Function for signal handling
void CatchSignals(int signo)
{
    finishFlag = 1;
    pthread_cond_broadcast(&cond_nonfull);
    pthread_cond_broadcast(&cond_nonempty);
}

//----------    FUNCTIONS    ----------//

//BoundedBuffer
void initializeBoundedBuffer(BoundedBuffer* boundedBuffer, int boundedBufferSize)
{
    boundedBuffer->fds = malloc(boundedBufferSize * sizeof(int));
    boundedBuffer->start = 0;
    boundedBuffer->end = -1;
    boundedBuffer->count = 0;
    boundedBuffer->size = boundedBufferSize;
    pthread_mutex_init(&bufferMutex, 0);
    pthread_cond_init(&cond_nonempty, 0);
    pthread_cond_init(&cond_nonfull, 0);
}

void placeToBuffer(BoundedBuffer* boundedBuffer, int acceptedFD)
{
    pthread_mutex_lock(&bufferMutex);
    while(boundedBuffer->count >= boundedBuffer->size)
    {
        printf("Found Buffer Full\n");
        pthread_cond_wait(&cond_nonfull, &bufferMutex);
    }
    boundedBuffer->end = (boundedBuffer->end + 1) % (boundedBuffer->size);
    boundedBuffer->fds[boundedBuffer->end] = acceptedFD;
    boundedBuffer->count++;
    pthread_cond_broadcast(&cond_nonempty);
    pthread_mutex_unlock(&bufferMutex);
}

int obtainFromBuffer(BoundedBuffer* boundedBuffer)
{
    int fd = 0;
    pthread_mutex_lock(&bufferMutex);
    while (boundedBuffer->count <= 0)
    {
        if(finishFlag == 1)
            return -1;
        printf("Found Buffer Empty\n");
        pthread_cond_wait(&cond_nonempty, &bufferMutex);
    }
    fd = boundedBuffer->fds[boundedBuffer->start];
    boundedBuffer->start = (boundedBuffer->start + 1) % (boundedBuffer->size);
    boundedBuffer->count--;
    pthread_cond_broadcast(&cond_nonfull);
    pthread_mutex_unlock(&bufferMutex);
    return fd;
}

Threads* initializeThreads(int numThreads)
{
    finishFlag = 0;
    Threads* threads = malloc(sizeof(Threads));

    threads->threadsArray = malloc(numThreads * sizeof(pthread_t));
    threads->count = 0;
    threads->numThreads = numThreads;
    threads->finish = FINISH_FALSE;
    
    pthread_mutex_init(&threadMutex, 0);


    for(int i = 0; i < numThreads; i++)
    {
        pthread_create(&threads->threadsArray[i], NULL, threadFunction, (void*)threads);
        pthread_detach(threads->threadsArray[i]);
    }
    workersPortsSize = 0;
    workersPorts = NULL;
    to_run = -1;
    countries = NULL;
    totalCountries = 0;
    return threads;
}

void* threadFunction(void* ptr)
{   
    //SIGNALS
    static struct sigaction act;
    act.sa_handler = CatchSignals;
    sigfillset(&(act.sa_mask));
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGINT);
    sigaddset(&(act.sa_mask), SIGQUIT);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);


    Threads* threads = (Threads*) ptr;
    while(threads->finish == FINISH_FALSE)
    {
        if(finishFlag == 1)
        {
            threads->finish = FINISH_TRUE;
            break;
        }
        int fd = obtainFromBuffer(&boundedBuffer);
        if(fd == -1)
        {
            threads->finish = FINISH_TRUE;
            break;
        }
        printf("Thread: %ld start processing accepted request with fd: %d\n", pthread_self(), fd);
            

        char* rcvd_message = NULL;
        if(to_run == 1)
        {
            int first_time = 0;
            while (1)
            {

                int msgTotalBytes = 0;
                read(fd, &msgTotalBytes, sizeof(int));
                //printf("totalBytes: %d\n", msgTotalBytes);
                rcvd_message = calloc(msgTotalBytes+1, sizeof(char));

                read_data(fd, rcvd_message, msgTotalBytes);
                //printf("msg rcvd: %s\n", rcvd_message);
                //sleep(1);

                if(strcmp("END", rcvd_message) == 0)
                {
                    free(rcvd_message);
                    //threads->finish = FINISH_TRUE;
                    printf("Closing connection.\n");
                    pthread_mutex_lock(&threadMutex);
                    //printStatsList(statsList);
                    addCountriesToArray();
                    pthread_mutex_unlock(&threadMutex);
                    close(fd);	  /* Close socket */
                    break;
                }
                else
                {
                    if(first_time == 0)
                    {
                        pthread_mutex_lock(&threadMutex);
                        printf("Worker port: %s\n", rcvd_message);
                        //create connection with worker
                        struct sockaddr_in worker;
                        struct sockaddr *workerptr = (struct sockaddr*)&worker;
                        //struct hostent *rem;
                        int sock;
                        /* Create socket */
                        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                        {
                        	perror_exit("socket");
                        }
                        //Find worker address
                        worker.sin_family = AF_INET;
                        int port = atoi(rcvd_message);
                        worker.sin_port = htons(port);
                        worker.sin_addr = clientAddr.sin_addr;
                        /* Initiate connection */
                        if (connect(sock, workerptr, sizeof(worker)) < 0)
                        {
	                       perror_exit("connect");
                        }
                        printf("Connecting to %s port %d\n", inet_ntoa(worker.sin_addr), port);
                        //add worker fd to array
                        workersPortsSize++;
                        workersFDs = realloc(workersFDs, workersPortsSize * sizeof(int));
                        workersFDs[workersPortsSize-1] = sock;
                        first_time = 1;
                        pthread_mutex_unlock(&threadMutex);
                    }
                    else
                    {
                        pthread_mutex_lock(&threadMutex);
                        storeStats(statsList, rcvd_message);
                        pthread_mutex_unlock(&threadMutex);
                    }
                }
                free(rcvd_message);
            }
        }
        else if(to_run == 2)
        {
            while (1)
            {
                int msgTotalBytes = 0;
                read(fd, &msgTotalBytes, sizeof(int));
                rcvd_message = calloc(msgTotalBytes+1, sizeof(char));

                read_data(fd, rcvd_message, msgTotalBytes);
                printf("Command received: %s\n", rcvd_message);

                //-----     RUN COMMANDS HERE ------//
                int commandCode = runCommands(rcvd_message, statsList, countries, totalCountries, workersFDs, workersPortsSize, fd);
                //////////////////////////////////////
                
                //write msg back
                //write_data(fd, rcvd_message);
                if(commandCode == EXIT || strcmp("END", rcvd_message) == 0)
                {
                    free(rcvd_message);
                    printf("Closing connection.\n");
                    close(fd);	  /* Close socket */
                    break;
                }
                free(rcvd_message);
            }
        }
    }
    destroyThreads(threads, threads->numThreads);
    printf("About to exit\n");
    exit(0);
    return 0;
}



void destroyThreads(Threads* threads, int numThreads)
{
    for(int i = 0; i < numThreads; i++)
    {
        pthread_join(threads->threadsArray[i], NULL);
    }
    for(int i = 0; i < numThreads; i++)
    {
        pthread_kill(threads->threadsArray[i], SIGKILL);
    }
  
    free(threads->threadsArray);
    free(threads);

   
    free(boundedBuffer.fds);
    freeStatsList(statsList);
    free(statsList);


    pthread_mutex_lock(&bufferMutex);
    pthread_mutex_destroy(&bufferMutex);
    pthread_cond_destroy(&cond_nonempty);
    pthread_cond_destroy(&cond_nonfull);
}

void addCountriesToArray()
{
    StatNodePointer current = statsList->headNode;
    while(current != NULL)
    {
        int flag = 0;
        if(totalCountries == 0)
        {
            totalCountries++;
            countries = realloc(countries, totalCountries * sizeof(*countries));
            countries[totalCountries-1] = malloc(sizeof(char) * (strlen(current->country)+1));
            strcpy(countries[totalCountries-1], current->country);
            current = current->next;
            continue;
        }

        for(int i = 0; i < totalCountries; i++)
        {
            if(strcmp(countries[i], current->country) == 0)
            {
                flag = 1;
                break;
            }
        }
        if(flag == 0)
        {
            totalCountries++;
            countries = realloc(countries, totalCountries * sizeof(*countries));
            countries[totalCountries-1] = malloc(sizeof(char) * (strlen(current->country)+1));
            strcpy(countries[totalCountries-1], current->country);
        }
        current = current->next;
    }
}