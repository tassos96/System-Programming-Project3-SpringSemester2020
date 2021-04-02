/*inet_str_server.c: Internet stream sockets server */
#define  _GNU_SOURCE
#include <stdio.h>
#include <sys/wait.h>	     /* sockets */
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <netdb.h>	         /* gethostbyaddr */
#include <unistd.h>	         /* fork */		
#include <stdlib.h>	         /* exit */
#include <ctype.h>	         /* toupper */
#include <signal.h>          /* signal */
#include <string.h>

#include "include/whoServer.h"
#include "include/serverCmdArguments.h"
#include "include/statsList.h"
#include "include/threads.h"



#define LISTEN_QUEUE_SIZE 10
#define MSG_BUFFERSIZE 500

int main(int argc, char const *argv[]) 
{
    //////////////////////////////////////
    int server_worker_Socket;
    int new_server_worker_Socket;
    int server_worker_Port = -1; //statisticsPortNum
    ///////////////////////////////////////
    int server_client_Socket;
    int new_server_client_Socket;
    int server_client_Port = -1; //queryPortNum
    ///////////////////////////////////////

    struct sockaddr_in server_workerAddr;
    struct sockaddr_in server_clientAddr;
    
    //struct sockaddr_in clientAddr;
    clientLen = sizeof(clientAddr);

    struct sockaddr* server_workerPtr = (struct sockaddr *) &server_workerAddr;
    struct sockaddr* server_clientPtr = (struct sockaddr *) &server_clientAddr; 
    clientPtr = (struct sockaddr *) &clientAddr;

    ///////////////////////////////////////
    
    //READ COMMAND LINE ARGS
    int numThreads = -1;
    int bufferSize = -1;

    //Read all arguments from command line and initialize the variables
    readCommandLineArguments(&server_client_Port, &server_worker_Port, &numThreads, &bufferSize, argc, argv);
    //Check for error (If some of args have not initialized, return error)
    checkForArgsError(server_client_Port, server_worker_Port, numThreads, bufferSize);

    //Print the args value
    printf("SERVER STARTED\n");
    printf("Clients Port: %d\n", server_client_Port);
    printf("Workers Port: %d\n", server_worker_Port);
    printf("Number of total threads: %d\n", numThreads);
    printf("Buffer Size: %d\n", bufferSize);
    
    ///////////////////////////////////////

    //Initialize bounded buffer
    initializeBoundedBuffer(&boundedBuffer, bufferSize);
    
    //SOCKETS CREATION

    if( (server_worker_Socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror_exit("Socket worker: ");
    }
    if( (server_client_Socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror_exit("Socket client: ");
    }

    setsockopt(server_worker_Socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    setsockopt(server_client_Socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    server_workerAddr.sin_family = AF_INET;
    server_workerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_workerAddr.sin_port = htons(server_worker_Port);

    server_clientAddr.sin_family = AF_INET;
    server_clientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_clientAddr.sin_port = htons(server_client_Port);
    ///////////////////////////////////////

    // BIND SOCKETS TO ADDRESSES
    if( bind(server_worker_Socket, server_workerPtr, sizeof(server_workerAddr)) < 0)
    {
        perror_exit("Bind worker: ");
    }
    if( bind(server_client_Socket, server_clientPtr, sizeof(server_clientAddr)) <0)
    {
        perror_exit("Bind client: ");
    }
    ///////////////////////////////////////

    // LISTEN FOR CONNECTIONS
    if( listen(server_worker_Socket, LISTEN_QUEUE_SIZE) < 0)
    {
        perror_exit("Listen worker: ");
    }
    printf("Listening for connections to port %d\n", server_worker_Port);
    
    if( listen(server_client_Socket, LISTEN_QUEUE_SIZE) < 0)
    {
        perror_exit("Listen client: ");
    }
    printf("Listening for connections to port %d\n", server_client_Port);
    ///////////////////////////////////////

    fd_set socketsFDs; //for select()

    Threads* threads = initializeThreads(numThreads);

    //Initialize list to store the statistics taken from workers
    statsList = calloc(1, sizeof(StatsList));
    statsList->headNode = initializeStatsList();
    statsList->tailNode = statsList->headNode;

    while (1)
    {
        if(finishFlag == 1)
        {
            freeStatsList(statsList);
            free(statsList);
            free(threads);
            break;
        }
        //ACCEPT CONNECTIONS FROM 2 PORTS
        FD_ZERO(&socketsFDs);
        FD_SET(server_worker_Socket, &socketsFDs);
        FD_SET(server_client_Socket, &socketsFDs);
        int maximumFD = -1;

        maximumFD = getMaximumFD(server_worker_Socket, server_client_Socket);
        int selectResult = select(maximumFD+1, &socketsFDs, NULL, NULL, NULL);
        if(selectResult > 0)
        {
            //GET ONE BETWEEN THE 2 PORTS
            if(FD_ISSET(server_worker_Socket, &socketsFDs))
            {
                if( (new_server_worker_Socket = accept(server_worker_Socket, clientPtr, &clientLen)) < 0)
                {
                    perror_exit("Accept worker: ");
                }
    	        printf("Accepted worker connection: fd->%d\n", new_server_worker_Socket);
                //Place accepted connection fd to buffer
                to_run = 1; //which block of code to run
                placeToBuffer(&boundedBuffer, new_server_worker_Socket);
            }
            if(FD_ISSET(server_client_Socket, &socketsFDs))
            {
                if( (new_server_client_Socket = accept(server_client_Socket, clientPtr, &clientLen)) < 0)
                {
                    perror_exit("Accept client: ");
                }
    	        printf("Accepted client connection: fd->%d\n", new_server_client_Socket);
                //Place accepted connection fd to buffer
                to_run = 2; //which block of code to run
                placeToBuffer(&boundedBuffer, new_server_client_Socket);
            }
        }
    }
    return 0;
}

void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

//Function to get maximum fd for select
int getMaximumFD(int fd1, int fd2)
{
    if(fd1 >= fd2)
    {
        return fd1;
    }
    else
    {
        return fd2;
    }
}

//Function to read from socket
void read_data (int fd, char *msg_buffer, int msgTotalBytes)
{
    // /* Read formated data */
    read(fd, msg_buffer, msgTotalBytes);
    (msg_buffer)[msgTotalBytes] = '\0';
}

//Function to write to socket
int write_data(int fd, char* msgToSend)
{
    //Write formated data
    int nwrite;
    size_t bytesToSend = strlen(msgToSend)+1;
    write(fd, &bytesToSend, sizeof(int));
    
    if((nwrite = write(fd, msgToSend, strlen(msgToSend)+1)) == -1)
    {
        perror("Error in writing");
        return -1;
    }
    return 1;
}

