#ifndef INTERFACE_H
#define INTERFACE_H

//Function to read commands from interface
int readCommandFromInterface(int* parentWriteFDs, int* parentReadFDs, int numWorkers, int bufferSize, char** countries, int totalCountries, sigset_t signal_set, int* total, int* success, int* fail, pid_t pids[]);

//Function for running commands
int runCommands(char** inputLine, int* parentWriteFDs, int* parentReadFDs, int numWorkers, int bufferSize, char** countries, int totalCountries, int* success, int* fail, pid_t pids[]);

//Function to count command arguments 
int countCommandArguments(char** command);

//Function to save command's arguments to a string array
char** getCommandArguments(char*** arguments, char** copyOfCommand, int argumentsNum);

//Function to free our string array
void freeArgumentsStringArray(char*** arguments, int argumentsNum);

//Function to print wrong command error
void printWrongCommandError();

//Function for parent's log file creation
void writeParentLogFile(char** countries, int totalCountries, int total, int success, int fail);

#endif