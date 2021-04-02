#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include  <errno.h> 
#include  <fcntl.h>
#include <signal.h>


#include "include/interface.h"
#include "include/parentOperations.h"
#include "include/fileOperations.h"

#define EXIT 7

#define ERROR -1

//Flag for signal handling
volatile sig_atomic_t stop = 0;

//Function for signal handling
void catchSignals(int signo)
{
    stop = 1;
}

//Function to read commands from interface
int readCommandFromInterface(int* parentWriteFDs, int* parentReadFDs, int numWorkers, int bufferSize, char** countries, int totalCountries, sigset_t signal_set, int* total, int* success, int* fail, pid_t pids[])
{
    //SIGNALS
    static struct sigaction act;
    act.sa_handler = catchSignals;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    ////////////////////////////////

    char* inputLine = NULL;
    size_t inputLength = 0;
    size_t lineSize = -1;

    //We can unblock signals here
    sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
    //If signal have arrived then write parent log file and proceed to termination
    if(stop == 1)
    {
        writeParentLogFile(countries, totalCountries, (*total), (*success), (*fail));
        return EXIT;
    }
    lineSize = getline(&inputLine, &inputLength, stdin);
    if(lineSize == -1 && stop == 1)
    {
        writeParentLogFile(countries, totalCountries, (*total), (*success), (*fail));
        free(inputLine);
        return EXIT;
    }
    //Block again signals here
    sigprocmask(SIG_BLOCK, &signal_set, NULL);
    if(lineSize == -1)
    {
        (*fail)++;
        printf("Error occured: Check again your command.\n");
        free(inputLine);
        return -1;
    }
    else
    {
        inputLine[strlen(inputLine)-1] = '\0';

        //Run commands here
        int commandCode = runCommands(&inputLine, parentWriteFDs, parentReadFDs, numWorkers, bufferSize, countries, totalCountries, success, total, pids);
        (*total)++;
        free(inputLine);
        return commandCode;
    }
}

//Function for running commands
int runCommands(char** inputLine, int* parentWriteFDs, int* parentReadFDs, int numWorkers, int bufferSize, char** countries, int totalCountries, int* success, int* fail, pid_t pids[])
{
    int functionReturnCode = -1;
    char* command = NULL;
    char* copyOfCommand = NULL;
    char** arguments = NULL;
    int argumentsNum = 0;

    copyOfCommand = malloc(strlen(*inputLine) * sizeof(char) + 1);
    strcpy(copyOfCommand, *inputLine);
    
    if(strcmp(copyOfCommand,"") == 0)
    {
        (*fail)++;
        printf("Please type a command\n");
        free(copyOfCommand);
        return ERROR;
    }
    command = strtok(*inputLine, " ");

    argumentsNum = countCommandArguments(&command);

    arguments = getCommandArguments(&arguments, &copyOfCommand, argumentsNum);

    free(copyOfCommand);

    if(strcmp(arguments[0], "/exit") == 0)
    {
        for(int i = 0; i < numWorkers; i++)
        {
            kill(pids[i], SIGINT);  //Send SIGINT to all workers to terminate them
        }
        functionReturnCode = EXIT;
        (*success)++;
    }

    freeArgumentsStringArray(&arguments, argumentsNum);


    if(functionReturnCode == -1)
    {
        printWrongCommandError();
    }
    return functionReturnCode;
}

/**
 * Function to count command arguments 
 **/
int countCommandArguments(char** command)
{
    int argumentsCounter = 0;   //includes command name
    while( *command != NULL ) 
    {
        argumentsCounter++;
        *command = strtok(NULL, " ");
    }
    return argumentsCounter;
}

/**
 * Function to save command's arguments to a string array
 **/
char** getCommandArguments(char*** arguments, char** copyOfCommand, int argumentsNum) 
{
    char* command = NULL;

    arguments[0] = malloc(sizeof(char*) * (argumentsNum));
    int i = 0;
    command = strtok(*copyOfCommand, " ");
    while(command != NULL) 
    {
        arguments[0][i] = malloc(sizeof(char) * strlen(command) + 1);
        strcpy(arguments[0][i], command);
        command = strtok(NULL, " ");
        i++;
    }
    return arguments[0];
}

/**
 * Function to free our string array
 **/
void freeArgumentsStringArray(char*** arguments, int argumentsNum) 
{
    for(int i = 0; i < argumentsNum; i++)
    {
        free(arguments[0][i]);
    }
    free(arguments[0]);
}

//Function to print wrong command error
void printWrongCommandError()
{
    printf("Error occured: Something goes wrong with command.\n");
    printf("Please try again\n");
}

//Function for parent's log file creation
void writeParentLogFile(char** countries, int totalCountries, int total, int success, int fail)
{
    pid_t parentID = getpid();
    int idDigits = howManyDigits(parentID);

    char* logFileName = malloc(sizeof(char*) * (strlen("./log_files/log_file.") + idDigits) + 1);
    sprintf(logFileName, "./log_files/log_file.%d", parentID);

    char* msgToLog = NULL;
    int totalBytes = 0;
    for(int i = 0; i < totalCountries; i++)
    {
        totalBytes += strlen(countries[i]) + strlen("\n");
    }
    int totalDigits = howManyDigits(total);
    int successDigits = howManyDigits(success);
    int failDigits = howManyDigits(fail);

    totalBytes += strlen("TOTAL") + totalDigits + strlen("\n");
    totalBytes += strlen("SUCCESS") + successDigits + strlen("\n");
    totalBytes += strlen("FAIL") + failDigits + strlen("\n");
    
    char* totalMsg = malloc(sizeof(char*) * (strlen("TOTAL ") + totalDigits + strlen("\n")) + 1);
    sprintf(totalMsg, "TOTAL %d\n", total);

    char* successMsg = malloc(sizeof(char*) * (strlen("SUCCESS ") + successDigits + strlen("\n")) + 1);
    sprintf(successMsg, "SUCCESS %d\n", success);

    char* failMsg = malloc(sizeof(char*) * (strlen("FAIL ") + failDigits + strlen("\n")) + 1);
    sprintf(failMsg, "FAIL %d\n", fail);

    //msgToLog = calloc(sizeof(char*) * totalBytes + 1);
    msgToLog = calloc(totalBytes + 1, sizeof(char*));
    for(int i = 0; i < totalCountries; i++)
    {
        strcat(msgToLog, countries[i]);
        strcat(msgToLog, "\n");
    }
    strcat(msgToLog, totalMsg);
    strcat(msgToLog, successMsg);
    strcat(msgToLog, failMsg);

    FILE* file;
    file = fopen(logFileName, "w");
    if(file == NULL)
    {
        perror("Parent LogFile");
        return;
    }

    fputs(msgToLog, file);

    free(logFileName);
    free(msgToLog);
    free(totalMsg);
    free(successMsg);
    free(failMsg);
    fclose(file);
    return;
}