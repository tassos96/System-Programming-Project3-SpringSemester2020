/**
 * This file contains methods for reading command line arguments.
**/
#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/clientCmdArguments.h"

#define ARGS_SIZE 9 //Correct number of command line total args

/**
 *  Function for reading arguments from command line and initalize variables on main.c 
 **/
void readCommandLineArguments(char** queryFileName, int* numThreads, int* serverPort, char** serverIP, int argc, const char* argv[])
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
            if(strcmp(argv[i], "-q") == 0)  //queryFileName
            {
                *queryFileName = malloc(strlen(argv[i+1]) * sizeof(char) + 1);
                strcpy(*queryFileName, argv[i+1]);
            }
            else if(strcmp(argv[i], "-w") == 0) //numThreads
            {
                char* endptr;
                *numThreads = strtol(argv[i+1], &endptr, 10);
                if(*endptr)//if the result is 0, error
                {
                    printArgsErrorMsg();
                    exit(1);
                }
            }
            else if(strcmp(argv[i], "-sp") == 0) //serverPort
            {
                char* endptr;
                *serverPort = strtol(argv[i+1], &endptr, 10);
                if(*endptr)//if the result is 0, error
                {
                    printArgsErrorMsg();
                    exit(1);
                }
            }
            else if(strcmp(argv[i], "-sip") == 0) //serverIP
            {
                *serverIP = malloc(strlen(argv[i+1]) * sizeof(char) + 1);
                strcpy(*serverIP, argv[i+1]);
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
void checkForArgsError(char* queryFileName, int numThreads, int serverPort, char* serverIP)
{
    if ( (queryFileName == NULL) || (numThreads == -1) || (numThreads == 0) || (serverPort == -1) || (serverIP == NULL) )
    {
        printArgsErrorMsg();    //Print errors
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