/**
 * This file contains methods for reading command line arguments.
**/
#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/serverCmdArguments.h"

#define ARGS_SIZE 9 //Correct number of command line total args

int whoClientsPort = -1;
    int workersPort = -1;
    int numThreads = -1;
    int bufferSize = -1;
/**
 *  Function for reading arguments from command line and initalize variables on main.c 
 **/
void readCommandLineArguments(int* whoClientsPort, int* workersPort, int* numThreads, int* bufferSize, int argc, const char* argv[])
{
    //Check if all needed arguments passed
    if(argc < ARGS_SIZE || argc > ARGS_SIZE)
    {
        printArgsErrorMsg();
        exit(1);    //Terminate program
    }
    else
    {
        for(int i = 1; i < argc - 1; i+=2)
        {
            if(strcmp(argv[i], "-q") == 0)  //whoClientsPort
            {
                char* endptr;
                *whoClientsPort = strtol(argv[i+1], &endptr, 10);
                if (*endptr) //if the result is 0, error
                {
                    printArgsErrorMsg();
                    exit(1);
                }
            }
            else if(strcmp(argv[i], "-s") == 0) //workersPort
            {
                char* endptr;
                *workersPort = strtol(argv[i+1], &endptr, 10);
                if(*endptr)//if the result is 0, error
                {
                    printArgsErrorMsg();
                    exit(1);
                }
            }
            else if(strcmp(argv[i], "-w") == 0) //number of total threads
            {
                char* endptr;
                *numThreads = strtol(argv[i+1], &endptr, 10);
                if(*endptr)//if the result is 0, error
                {
                    printArgsErrorMsg();
                    exit(1);
                }
            }
            else if(strcmp(argv[i], "-b") == 0) //size of buffer
            {
                char* endptr;
                *bufferSize = strtol(argv[i+1], &endptr, 10);
                if (*endptr) //if the result is 0, error
                {
                    printArgsErrorMsg();
                    exit(1);
                }
            }
            else
            {
                printArgsErrorMsg();
                exit(1);
            }
        }
    }
}

/**
 * Function to make sure every argument needed passed from command line 
 **/
void checkForArgsError(int whoClientsPort, int workersPort, int numThreads, int bufferSize)
{
    if ( (whoClientsPort == -1) || (workersPort == -1) || (numThreads == -1) || (bufferSize == -1) )
    {
        printArgsErrorMsg();    //Print error
        exit(2);                //Terminate program
    }  
}

/**
 * Function to print error messages for command line arguments
 **/
void printArgsErrorMsg()
{
    printf("Error occured: Something is wrong with arguments. \n");
    printf("Please try again.\n");
}