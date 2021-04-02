#ifndef SERVER_CMD_ARGUMENTS_H
#define SERVER_CMD_ARGUMENTS_H

// Function for reading arguments from command line and initalize variables on main.c 
void readCommandLineArguments(int* whoClientsPort, int* workersPort, int* numThreads, int* bufferSize, int argc, const char* argv[]);
//Function to make sure every argument needed passed from command line 
void checkForArgsError(int whoClientsPort, int workersPort, int numThreads, int bufferSize);
//Function to print error messages for command line arguments
void printArgsErrorMsg();

#endif