#ifndef PARENT_OPERATIONS_H
#define PARENT_OPERATIONS_H

//==============================================    PARENT    ==============================================//

//Function for parent to open his pipes that already has been created
int parentOpenPipes(int numWorkers, int** parentWriteFDs, int** parentReadFDs);

//Function for all the parent operations
int parentOperations(int numWorkers, int* parentWriteFDs, int* parentReadFds, char** directoriesNames, int totalDirectories, int bufferSize, char* serverIP, int serverPort, int input_dirLen, pid_t pids[]);

//Function for writting message to worker
int writeToWorker(int writeFd, char* msgToSend);

//Function for message reading from worker
int readFromWorker(int readFd,  int msgTotalBytes, char* message, int bufferSize);

//Function to get maximum fd for select
void getMaximumFD(int* maximumFD, int* parentWriteFDs, int* parentReadFDs, int numWorkers);

#endif