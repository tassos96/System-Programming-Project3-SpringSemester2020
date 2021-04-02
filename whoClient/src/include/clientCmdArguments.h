#ifndef CLIENT_CMD_ARGUMENTS_H
#define CLIENT_CMD_ARGUMENTS_H

// Function for reading arguments from command line and initalize variables on main.c 
void readCommandLineArguments(char** queryFileName, int* numThreads, int* serverPort, char** serverIP, int argc, const char* argv[]);
//Function to make sure every argument needed passed from command line 
void checkForArgsError(char* queryFileName, int numThreads, int serverPort, char* serverIP);
//Function to print error messages for command line arguments
void printArgsErrorMsg();

#endif