#define  _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <unistd.h>          /* read, write, close */
#include <netdb.h>	         /* gethostbyaddr */
#include <stdlib.h>	         /* exit */
#include <string.h>	         /* strlen */
#include <pthread.h>
#include <unistd.h>

#include "include/clientCmdArguments.h"

#define MSG_BUFFERSIZE 256

void perror_exit(char *message);
void read_data (int fd, char *msg_buffer, int msgTotalBytes);
int write_data(int fd, char* message);
void* threadFunction();

//For reading from query file
FILE* filePtr;
char buf[MSG_BUFFERSIZE];

//Thread mutex
pthread_mutex_t thread_mtx;

//For sockets connection
struct sockaddr_in server;
struct sockaddr *serverptr = (struct sockaddr*)&server;
struct hostent *rem;

//Save file lines to a string array
int num_of_file_lines = 0;
char** file_lines_array = NULL;

//Thread argument struct
typedef struct ArgumentLines{
    int numLines;
    char** lines;
}ArgumentLines;

//Main function
int main(int argc, char const *argv[])
{
    //READ COMMAND LINE ARGS
    char* queryFileName = NULL; //queryFileName
    int numThreads = -1; //threadsNum
    int serverPort = -1; //whoServer Port
    char* serverIP = NULL; //whoServer IP

    //Read all arguments from command line and initialize the variables
    readCommandLineArguments(&queryFileName, &numThreads, &serverPort, &serverIP, argc, argv);
    //Check for error (If some of args have not initialized, return error)
    checkForArgsError(queryFileName, numThreads, serverPort, serverIP);

    //Print the args value
    printf("CLIENT STARTED\n");
    printf("Query file: %s\n", queryFileName);
    printf("Number of total threads: %d\n", numThreads);
    printf("Server Port: %d\n", serverPort);
    printf("Server IP: %s\n", serverIP);
    ///////////////////////////////////////

	/* Find server address */
    if ((rem = gethostbyname(serverIP)) == NULL) 
    {	
	   herror("gethostbyname");
       exit(1);
    }
    
    server.sin_family = AF_INET;       /* Internet domain */
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);

    /* Server port */
    server.sin_port = htons(serverPort);         

    //OPEN FILE
    if( ( filePtr = fopen(queryFileName, "r" ) ) == NULL )
    {
        perror("Read query file: ");
        free(queryFileName);
        free(serverIP);
        return -1;
    }
    
    //READ FILE LINES AND SAVE THEM TO STRING ARRAY
    while ( fgets((char *)&buf, sizeof(buf), filePtr) )
    {           
        buf[strlen(buf) - 1] = '\0';

        num_of_file_lines++;
        file_lines_array = realloc(file_lines_array, num_of_file_lines * (sizeof(*file_lines_array)) );
        file_lines_array[num_of_file_lines-1] = malloc(sizeof(char) * (strlen(buf)+1 ));
        strcpy(file_lines_array[num_of_file_lines-1], buf);
    }
    
    //Divide command lines from file to the argument structs
    struct ArgumentLines** args = NULL;
    args = calloc(numThreads, sizeof(struct ArgumentLines*));
    for(int i = 0; i < numThreads; i++)
    {
        args[i] = calloc(1, sizeof(struct ArgumentLines));
    }

    int current_thread = 0;
    for(int curLine = 0; curLine < num_of_file_lines; curLine++)
    {
        if(current_thread == numThreads)
        {
            current_thread = 0;
        }
        //printf("Thread_%d, %s\n",current_thread ,file_lines_array[curLine]);
        
        args[current_thread]->numLines++;
        args[current_thread]->lines = realloc(args[current_thread]->lines, args[current_thread]->numLines * sizeof(*(args[current_thread]->lines)) );
        args[current_thread]->lines[args[current_thread]->numLines - 1] = malloc(sizeof(char) * strlen(file_lines_array[curLine]) +1);
        strcpy(args[current_thread]->lines[args[current_thread]->numLines - 1], file_lines_array[curLine]);
 
        current_thread++;
    }
    /////////////////////////////////////////

    //THREAD ARRAY AND MUTEX INIT
    pthread_t threads[numThreads];
    pthread_mutex_init(&thread_mtx, 0);
    
    //Create num threads
    for(int i = 0; i < numThreads; i++)
    {
        pthread_create(&threads[i], NULL, threadFunction, args[i]);
    }
    ////////////

    //Wait for threads to join so main process can finish
    for(int i = 0; i < numThreads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    //Close file, free allocated memory and exit
    printf("Close file, free allocated memory\n");
    for(int i = 0; i < num_of_file_lines; i++)
    {
        free(file_lines_array[i]);
    }
    free(file_lines_array);
    for(int i = 0; i < numThreads; i++)
    {
        for(int curLine = 0; curLine < args[i]->numLines; curLine++)
        {
            free(args[i]->lines[curLine]);
        }
        free(args[i]->lines);
        free(args[i]);
    }
    free(args);
    fclose(filePtr);
    free(queryFileName);
    free(serverIP);
    printf("Client exits\n");
    return 0;
}

//This the thread function for each thread
void* threadFunction(void* args)
{
    pthread_mutex_lock(&thread_mtx);
    ArgumentLines* arg = (ArgumentLines*) args;
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    	perror_exit("socket");
    }   
    //////////////////
    if (connect(sock, serverptr, sizeof(server)) < 0)
    {
	    perror_exit("connect");
    }
    
    char* rcvd_message = NULL;
    for(int curLine = 0; curLine < arg->numLines; curLine++)
    {
        printf("Command: %s\n", arg->lines[curLine]);
        
        if (write_data(sock, arg->lines[curLine]) < 0)
        {
           perror_exit("write");
        }

        /* receive msg back */
        int msgTotalBytes = 0;
        read(sock, &msgTotalBytes, sizeof(int));
        rcvd_message = calloc(msgTotalBytes+1, sizeof(char));
        read_data(sock, rcvd_message, msgTotalBytes);
        //Print reply
        printf("%s\n", rcvd_message);
    
        if(strcmp(rcvd_message, "END") == 0)
        {
            free(rcvd_message);
        }
        free(rcvd_message);
    }

    //Send exit message to server
    write_data(sock, "/exit");
    //close(sock);

    pthread_mutex_unlock(&thread_mtx);

    return 0;
}

//Function to read data from socket
void read_data (int fd, char *msg_buffer, int msgTotalBytes)
{
    read(fd, msg_buffer, msgTotalBytes);
    (msg_buffer)[msgTotalBytes] = '\0';
}

//Function to write data to socket
int write_data(int fd, char* msgToSend)
{
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

//Function for printing errors
void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}